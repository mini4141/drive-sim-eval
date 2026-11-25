#!/usr/bin/env bash
# Tear everything down. Idempotent: safe to re-run, works even if Terraform state was lost.
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
export AWS_PROFILE=drive-sim-eval-deployer AWS_REGION=ap-southeast-2 AWS_DEFAULT_REGION=ap-southeast-2
export AWS_PAGER=""
P=drive-sim-eval
QUEUE="$P-queue"; CE="$P-fargate"
ACCOUNT="$(aws sts get-caller-identity --query Account --output text | tr -d '\r')"

echo "== 1/4 cancel and terminate Batch jobs"
for status in SUBMITTED PENDING RUNNABLE STARTING RUNNING; do
  for id in $(aws batch list-jobs --job-queue "$QUEUE" --job-status "$status" --query 'jobSummaryList[].jobId' --output text 2>/dev/null | tr -d '\r'); do
    echo "  terminating $id ($status)"
    aws batch terminate-job --job-id "$id" --reason "destroy.sh" || true
  done
done

echo "== 2/4 disable and delete the job queue, then the compute environment"
if aws batch describe-job-queues --job-queues "$QUEUE" --query 'jobQueues[0].jobQueueName' --output text 2>/dev/null | grep -q "$QUEUE"; then
  aws batch update-job-queue --job-queue "$QUEUE" --state DISABLED >/dev/null || true
  for _ in $(seq 1 30); do
    st="$(aws batch describe-job-queues --job-queues "$QUEUE" --query 'jobQueues[0].status' --output text | tr -d '\r')"
    [ "$st" = "VALID" ] && break; sleep 5
  done
  aws batch delete-job-queue --job-queue "$QUEUE" || true
  for _ in $(seq 1 30); do
    aws batch describe-job-queues --job-queues "$QUEUE" --query 'jobQueues[0].jobQueueName' --output text | grep -q "$QUEUE" || break; sleep 5
  done
fi
if aws batch describe-compute-environments --compute-environments "$CE" --query 'computeEnvironments[0].computeEnvironmentName' --output text 2>/dev/null | grep -q "$CE"; then
  aws batch update-compute-environment --compute-environment "$CE" --state DISABLED >/dev/null || true
  sleep 10
  aws batch delete-compute-environment --compute-environment "$CE" || true
fi

echo "== 3/4 empty the buckets and the ECR repository"
for b in "$P-data-$ACCOUNT" "$P-site-$ACCOUNT"; do
  if aws s3api head-bucket --bucket "$b" 2>/dev/null; then aws s3 rm "s3://$b" --recursive --only-show-errors || true; fi
done
if aws ecr describe-repositories --repository-names "$P" >/dev/null 2>&1; then
  ids="$(aws ecr list-images --repository-name "$P" --query 'imageIds[*]' --output json)"
  [ "$ids" != "[]" ] && aws ecr batch-delete-image --repository-name "$P" --image-ids "$ids" >/dev/null || true
fi

echo "== 4/4 terraform destroy"
terraform -chdir=infra init -input=false >/dev/null
terraform -chdir=infra destroy -auto-approve -input=false
echo "destroy complete"
