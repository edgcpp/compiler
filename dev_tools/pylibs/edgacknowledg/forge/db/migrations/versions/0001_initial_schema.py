# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Initial schema.

Revision ID: 0001
Revises:
Create Date: 2026-10-15
'''

import sqlalchemy as sa

from alembic import op

revision = '0001'
down_revision = None
branch_labels = None
depends_on = None


def _pull_request_fk() -> sa.ForeignKeyConstraint:
  return sa.ForeignKeyConstraint(
    ['pull_request_id'],
    ['pull_requests.id'],
    ondelete = 'CASCADE'
  )


def upgrade() -> None:
  # Datetimes are stored as naive UTC (see models.UTCDateTime).
  op.create_table(
    'pull_requests',
    sa.Column('id', sa.Integer(), nullable = False),
    sa.Column('forge', sa.String(length = 32), nullable = False),
    sa.Column('repository', sa.String(length = 255), nullable = False),
    sa.Column('number', sa.Integer(), nullable = False),
    sa.Column('review_id', sa.Uuid(), nullable = False),
    sa.Column('state', sa.String(length = 16), nullable = False),
    sa.Column('title', sa.Text(), nullable = False),
    sa.Column('author', sa.String(length = 255), nullable = False),
    sa.Column('url', sa.Text(), nullable = False),
    sa.Column('base_ref', sa.String(length = 255), nullable = False),
    sa.Column('base_sha', sa.String(length = 64), nullable = False),
    sa.Column('head_sha', sa.String(length = 64), nullable = False),
    sa.Column('fetch_ref', sa.String(length = 255), nullable = False),
    sa.Column('forge_updated_at', sa.DateTime(), nullable = False),
    sa.Column('head_changed_at', sa.DateTime(), nullable = False),
    sa.Column('settle_deadline', sa.DateTime(), nullable = False),
    sa.Column('generation', sa.Integer(), nullable = False),
    sa.Column('processed_generation', sa.Integer(), nullable = False),
    sa.Column('error_count', sa.Integer(), nullable = False),
    sa.Column('next_attempt_at', sa.DateTime(), nullable = True),
    sa.Column('last_error', sa.Text(), nullable = True),
    sa.PrimaryKeyConstraint('id'),
    sa.UniqueConstraint('forge', 'repository', 'number'),
    sa.UniqueConstraint('review_id'),
  )
  op.create_table(
    'announcements',
    sa.Column('pull_request_id', sa.Integer(), nullable = False),
    sa.Column('fingerprint', sa.String(length = 128), nullable = True),
    sa.Column('sections', sa.JSON(), nullable = False),
    sa.Column('pending_fingerprint', sa.String(length = 128), nullable = True),
    sa.Column('last_comment_id', sa.String(length = 64), nullable = True),
    sa.Column('last_posted_at', sa.DateTime(), nullable = True),
    _pull_request_fk(),
    sa.PrimaryKeyConstraint('pull_request_id'),
  )
  op.create_table(
    'ci_runs',
    sa.Column('pull_request_id', sa.Integer(), nullable = False),
    sa.Column('run_id', sa.String(length = 64), nullable = False),
    sa.Column('attempt', sa.Integer(), nullable = False),
    sa.Column('head_sha', sa.String(length = 64), nullable = False),
    sa.Column('status', sa.String(length = 16), nullable = False),
    sa.Column('conclusion', sa.String(length = 16), nullable = True),
    sa.Column('completed_at', sa.DateTime(), nullable = True),
    sa.Column('url', sa.Text(), nullable = False),
    sa.Column('artifacts', sa.JSON(), nullable = False),
    _pull_request_fk(),
    sa.PrimaryKeyConstraint('pull_request_id'),
  )
  op.create_table(
    'sections',
    sa.Column('id', sa.Integer(), nullable = False),
    sa.Column('pull_request_id', sa.Integer(), nullable = False),
    sa.Column('kind', sa.String(length = 32), nullable = False),
    sa.Column('name', sa.String(length = 255), nullable = False),
    sa.Column('slug', sa.String(length = 255), nullable = False),
    sa.Column('source_key', sa.Text(), nullable = False),
    sa.Column(
      'content_fingerprint',
      sa.String(length = 128),
      nullable = False
    ),
    sa.Column('root', sa.Text(), nullable = False),
    sa.Column('home', sa.Text(), nullable = False),
    sa.Column('details', sa.JSON(), nullable = False),
    _pull_request_fk(),
    sa.PrimaryKeyConstraint('id'),
    sa.UniqueConstraint('pull_request_id', 'slug'),
  )


def downgrade() -> None:
  op.drop_table('sections')
  op.drop_table('ci_runs')
  op.drop_table('announcements')
  op.drop_table('pull_requests')
