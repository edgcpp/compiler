# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Alembic environment: migrates the connection ForgeStore passes in.

Run through ``ForgeStore.open``; there is no standalone alembic.ini. To
write a new revision, run from this directory, e.g.:

  alembic -x url=sqlite:///new.db revision --autogenerate -m "..."
'''

from alembic import context
from sqlalchemy import create_engine
from sqlalchemy.engine import Connection

from edgacknowledg.forge.db.models import Base

config = context.config


def _run(connection: Connection) -> None:
  context.configure(
    connection = connection,
    target_metadata = Base.metadata,
    # SQLite can only alter tables by copying them.
    render_as_batch = True,
  )
  with context.begin_transaction():
    context.run_migrations()


connection = config.attributes.get('connection')
if connection is not None:
  _run(connection)
else:
  url = context.get_x_argument(as_dictionary = True)['url']
  with create_engine(url).connect() as standalone_connection:
    _run(standalone_connection)
