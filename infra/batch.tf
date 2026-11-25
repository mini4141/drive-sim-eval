resource "aws_batch_compute_environment" "fargate" {
  name         = "${var.project}-fargate"
  type         = "MANAGED"
  state        = "ENABLED"
  service_role = aws_iam_role.batch_service.arn

  compute_resources {
    type               = "FARGATE"
    max_vcpus          = 16
    subnets            = aws_subnet.public[*].id
    security_group_ids = [aws_security_group.batch.id]
  }

  depends_on = [aws_iam_role_policy_attachment.batch_service]
}

resource "aws_batch_job_queue" "main" {
  name     = "${var.project}-queue"
  state    = "ENABLED"
  priority = 1

  compute_environment_order {
    order               = 1
    compute_environment = aws_batch_compute_environment.fargate.arn
  }
}

# One definition serves the array children (batch-shard) and the aggregate job (command overridden at submit).
resource "aws_batch_job_definition" "sim" {
  name                  = "${var.project}-job"
  type                  = "container"
  platform_capabilities = ["FARGATE"]

  container_properties = jsonencode({
    image            = "${aws_ecr_repository.main.repository_url}:${var.image_tag}"
    command          = ["--help"]
    jobRoleArn       = aws_iam_role.job.arn
    executionRoleArn = aws_iam_role.execution.arn
    resourceRequirements = [
      { type = "VCPU", value = "1" },
      { type = "MEMORY", value = "2048" },
    ]
    fargatePlatformConfiguration = { platformVersion = "LATEST" }
    networkConfiguration         = { assignPublicIp = "ENABLED" }
    runtimePlatform              = { operatingSystemFamily = "LINUX", cpuArchitecture = "X86_64" }
    environment                  = [{ name = "AWS_REGION", value = var.region }]
    logConfiguration = {
      logDriver = "awslogs"
      options = {
        "awslogs-group"         = aws_cloudwatch_log_group.batch.name
        "awslogs-region"        = var.region
        "awslogs-stream-prefix" = "sim"
      }
    }
  })
}
