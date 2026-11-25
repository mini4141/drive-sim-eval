#!/usr/bin/env bash
# Exit non-zero if anything billable tagged Project=drive-sim-eval (or named drive-sim-eval-*) remains.
# Direct describe calls are authoritative; the tagging API can lag after deletion, so its hits are notes.
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
export AWS_PROFILE=drive-sim-eval-deployer AWS_PAGER=""
R=ap-southeast-2
P=drive-sim-eval
redact() { sed -E 's/[0-9]{12}/<ACCOUNT_ID>/g'; }

run_checks() {
  local fail=0
  echo "verify-destroyed $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  check() { # label, output (empty or None = gone)
    if [ -z "$2" ] || [ "$2" = "None" ]; then echo "OK    $1: none"; else echo "FOUND $1: $2"; fail=1; fi
  }
  echo "-- direct checks (authoritative)"
  check "batch job queues" "$(aws batch describe-job-queues --region $R --query "jobQueues[?starts_with(jobQueueName,'$P')].jobQueueName" --output text | tr -d '\r')"
  check "batch compute environments" "$(aws batch describe-compute-environments --region $R --query "computeEnvironments[?starts_with(computeEnvironmentName,'$P')].computeEnvironmentName" --output text | tr -d '\r')"
  check "ECR repositories" "$(aws ecr describe-repositories --region $R --query "repositories[?repositoryName=='$P'].repositoryName" --output text 2>/dev/null | tr -d '\r')"
  check "CloudFront distributions" "$(aws cloudfront list-distributions --query "DistributionList.Items[?contains(Comment,'$P')].Id" --output text 2>/dev/null | tr -d '\r')"
  check "VPCs tagged Project=$P" "$(aws ec2 describe-vpcs --region $R --filters Name=tag:Project,Values=$P --query 'Vpcs[].VpcId' --output text | tr -d '\r')"
  check "S3 buckets" "$(aws s3api list-buckets --query "Buckets[?starts_with(Name,'$P-')].Name" --output text | tr -d '\r')"
  check "CloudWatch log groups" "$(aws logs describe-log-groups --region $R --log-group-name-prefix /aws/batch/$P --query 'logGroups[].logGroupName' --output text | tr -d '\r')"
  check "IAM roles" "$(aws iam list-roles --query "Roles[?starts_with(RoleName,'$P-')].RoleName" --output text | tr -d '\r')"

  echo "-- tagging API (may lag a few minutes after deletion)"
  for region in $R us-east-1; do
    hits="$(aws resourcegroupstaggingapi get-resources --region $region --tag-filters Key=Project,Values=$P --query 'ResourceTagMappingList[].ResourceARN' --output text | tr -d '\r')"
    if [ -z "$hits" ]; then echo "OK    tagging API ($region): no resources"; else echo "NOTE  tagging API ($region) still lists: $hits"; fi
  done
  if [ "$fail" = 0 ]; then echo "RESULT: nothing billable remains"; else echo "RESULT: resources remain (see FOUND lines)"; fi
  return $fail
}

run_checks 2>&1 | redact | tee "${VERIFY_OUT:-/dev/null}"
exit "${PIPESTATUS[0]}"
