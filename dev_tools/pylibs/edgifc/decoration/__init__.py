# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

from edgifc import StreamReader

UNKNOWN_VALUE = '***UNKNOWN***'
UNSUPPORTED_VALUE = '***UNSUPPORTED: {}***'

class DecoratorMap:
  '''This type maps IFC types to their corresponding registered
  decorator(s).'''
  def __init__(self):
    self._decorator = {}

  def add_decorator(self, type_name, decorator):
    if type_name not in self._decorator:
      self._decorator[type_name] = []
    self._decorator[type_name].append(decorator)

  def __contains__(self, key):
    return key in self._decorator

  def __getitem__(self, key):
    return self._decorator[key]

def full_decoration_map(ifc_reader) -> DecoratorMap:
  decorator_map = DecoratorMap()
  # Register special decorators.
  decorator_map.add_decorator(
    'TextOffset',
    TextOffsetTextDecorator()
  )
  decorator_map.add_decorator(
    'NameIndex',
    NameIndexValueDecorator()
  )
  decorator_map.add_decorator(
    'LineOffset',
    SourceFileDecorator()
  )
  decorator_map.add_decorator(
    'LineOffset',
    SourceLineNoDecorator()
  )

  # Automatically generate various decorators
  for simple_sort in ifc_reader.get_all_registered_simple_sorts():
    sort_type = f"{simple_sort}Sort"
    decorator_map.add_decorator(
      sort_type,
      SimpleSortDecorator(simple_sort)
    )

  for index_sort in ifc_reader.get_all_registered_index_sorts():
    index_type = f"{index_sort}Index"
    decorator_map.add_decorator(
      index_type,
      IndexSortDecorator(index_sort)
    )
    decorator_map.add_decorator(
      index_type,
      IndexValueDecorator(index_sort)
    )

  for bitfield in ifc_reader.get_all_registered_bitfields():
    bitfield_type = f"{bitfield}Bitfield"
    decorator_map.add_decorator(
      bitfield_type,
      BitfieldDecorator(bitfield)
    )

  return decorator_map

class BitfieldDecorator:
  def __init__(self, bitfield_name):
    self.bitfield_name = bitfield_name

  def name(self):
    return 'bitfield'

  def decorate(self, ifc_reader, value):
    values = ifc_reader.get_bitfield_values(self.bitfield_name, value)
    return ' | '.join(values)

class TextOffsetTextDecorator:
  def name(self):
    return 'text'

  def decorate(self, ifc_reader, value):
    return StreamReader(ifc_reader).read_text(value)

class SimpleSortDecorator:
  def __init__(self, sort_name):
    self.sort_name = sort_name

  def name(self):
    return 'sort'

  def decorate(self, ifc_reader, value):
    return ifc_reader.get_sort_tag(self.sort_name, value)

class IndexSortDecorator:
  def __init__(self, sort_name):
    self.sort_name = sort_name

  def name(self):
    return 'sort'

  def decorate(self, ifc_reader, value):
    try:
      return ifc_reader.get_decomposed_index_sort(self.sort_name, value)[0]
    except Exception:
      sort, _ = ifc_reader.get_decomposed_index_sort_values(
        self.sort_name, value
      )
      return hex(sort)

class IndexValueDecorator:
  def __init__(self, sort_name):
    self.sort_name = sort_name

  def name(self):
    return 'value'

  def decorate(self, ifc_reader, value):
    try:
      return ifc_reader.get_decomposed_index_sort(self.sort_name, value)[1]
    except Exception:
      _, idx = ifc_reader.get_decomposed_index_sort_values(
        self.sort_name, value
      )
      return idx

class NameIndexValueDecorator:
  def name(self):
    return 'text'

  def decorate(self, ifc_reader, value):
    tag, index = ifc_reader.get_decomposed_index_sort('Name', value)
    if tag == 'TextOffset':
      return StreamReader(ifc_reader).read_text(index)
    if tag == 'NameSourceFile':
      stream_reader = StreamReader(ifc_reader)
      source_file = stream_reader.read('name.source-file', index)
      return StreamReader(ifc_reader).read_text(source_file.path())
    if tag == 'NameLiteral':
      stream_reader = StreamReader(ifc_reader)
      literal = stream_reader.read('name.literal', index)
      return StreamReader(ifc_reader).read_text(literal.encoded())
    return UNKNOWN_VALUE

class SourceFileDecorator:
  def __init__(self):
    self.name_decorator = NameIndexValueDecorator()

  def name(self):
    return 'filename'

  def decorate(self, ifc_reader, value):
    stream_reader = StreamReader(ifc_reader)
    file_and_line = stream_reader.read('src.line', value)
    # Delegate to a name decorator to finish the name
    return self.name_decorator.decorate(ifc_reader, file_and_line.file())

class SourceLineNoDecorator:
  def name(self):
    return 'lineno'

  def decorate(self, ifc_reader, value):
    stream_reader = StreamReader(ifc_reader)
    file_and_line = stream_reader.read('src.line', value)
    return file_and_line.line()

