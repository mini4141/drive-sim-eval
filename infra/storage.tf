resource "aws_ecr_repository" "main" {
  name         = var.project
  force_delete = true
}

# Scenarios (input)
resource "aws_s3_bucket" "data" {
  bucket        = "${var.project}-data-${data.aws_caller_identity.current.account_id}"
  force_destroy = true
}

# Viewer (site root) + results/ prefix, served through CloudFront
resource "aws_s3_bucket" "site" {
  bucket        = "${var.project}-site-${data.aws_caller_identity.current.account_id}"
  force_destroy = true
}

resource "aws_s3_bucket_public_access_block" "data" {
  bucket                  = aws_s3_bucket.data.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_s3_bucket_public_access_block" "site" {
  bucket                  = aws_s3_bucket.site.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_cloudwatch_log_group" "batch" {
  name              = "/aws/batch/${var.project}"
  retention_in_days = 1
}
