#!/usr/bin/env bash
# Deploy to AWS from this PC: terraform apply -> push image -> upload scenarios -> Batch array job
# -> aggregate job -> build + publish the viewer. Prints the CloudFront URL.
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
export AWS_PROFILE=drive-sim-eval-deployer AWS_REGION=ap-southeast-2 AWS_DEFAULT_REGION=ap-southeast-2
export AWS_PAGER=""
BATCH_ID="${BATCH_ID:-cloud-001}"
mkdir -p .local
tfout() { terraform -chdir=infra output -raw "$1" | tr -d '\r'; }

aws sts get-caller-identity --query Arn --output text | sed -E 's/[0-9]{12}/<ACCOUNT_ID>/g'

echo "== 1/7 terraform apply"
terraform -chdir=infra init -input=false
terraform -chdir=infra apply -auto-approve -input=false

ECR="$(tfout ecr_repository_url)"; DATA="$(tfout data_bucket)"; SITE="$(tfout site_bucket)"
QUEUE="$(tfout job_queue)"; JOBDEF="$(tfout job_definition)"; CF_DOMAIN="$(tfout cloudfront_domain)"
CF_ID="$(tfout cloudfront_id)"

echo "== 2/7 build and push image"
bash scripts/build-core.sh >/dev/null
aws ecr get-login-password | docker login --username AWS --password-stdin "${ECR%%/*}"
docker tag "$RUNTIME_IMAGE" "$ECR:latest"
docker push "$ECR:latest"

echo "== 3/7 upload scenarios and local demo results"
[ -d scenarios ] && [ "$(ls scenarios | wc -l)" -ge 200 ] || bash scripts/seed.sh
aws s3 sync scenarios "s3://$DATA/scenarios/" --only-show-errors
aws s3 sync results/demo "s3://$SITE/results/demo/" --only-show-errors
aws s3 cp results/repro.json "s3://$SITE/results/repro.json" --only-show-errors

echo "== 4/7 submit array job (20 children x 10 scenarios)"
OUT="s3://$SITE/results/$BATCH_ID"
cat >.local/shard-overrides.json <<JSON
{"command":["batch-shard","--shard-size","10","--in","s3://$DATA/scenarios","--out","$OUT","--config","configs/open_loop.yaml"]}
JSON
ARRAY_ID="$(aws batch submit-job --job-name "sim-shards-$BATCH_ID" --job-queue "$QUEUE" \
  --job-definition "$JOBDEF" --array-properties size=20 \
  --container-overrides file://.local/shard-overrides.json --query jobId --output text | tr -d '\r')"
echo "array job: $ARRAY_ID"

echo "== 5/7 submit dependent aggregate job"
cat >.local/agg-overrides.json <<JSON
{"command":["aggregate","--out","$OUT"]}
JSON
AGG_ID="$(aws batch submit-job --job-name "sim-aggregate-$BATCH_ID" --job-queue "$QUEUE" \
  --job-definition "$JOBDEF" --depends-on "jobId=$ARRAY_ID" \
  --container-overrides file://.local/agg-overrides.json --query jobId --output text | tr -d '\r')"
echo "aggregate job: $AGG_ID"
echo "$ARRAY_ID" >.local/array-job-id.txt; echo "$AGG_ID" >.local/aggregate-job-id.txt

wait_job() {
  local id="$1" status
  for _ in $(seq 1 120); do
    status="$(aws batch describe-jobs --jobs "$id" --query 'jobs[0].status' --output text | tr -d '\r')"
    echo "  $id: $status"
    case "$status" in SUCCEEDED) return 0 ;; FAILED) return 1 ;; esac
    sleep 15
  done
  return 1
}
wait_job "$ARRAY_ID" && wait_job "$AGG_ID"

echo "== 6/7 build and publish the viewer"
(cd viewer && npm ci --silent && VITE_RESULTS_BASE_URL=/results npm run build)
aws s3 sync viewer/dist "s3://$SITE/" --delete --exclude "results/*" --only-show-errors

echo "== 7/7 wait for CloudFront"
aws cloudfront wait distribution-deployed --id "$CF_ID"
echo "https://$CF_DOMAIN" >.local/cloud-url.txt
echo "$BATCH_ID" >.local/cloud-batch.txt
echo "Viewer: https://$CF_DOMAIN  (batch $BATCH_ID)"
