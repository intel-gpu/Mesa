#!/usr/bin/env python3
# Copyright © 2019, 2022 Intel Corporation
# SPDX-License-Identifier: MIT

from __future__ import annotations
from collections import OrderedDict
import copy
import pathlib
import os.path
import re
import xml.etree.ElementTree as et
import typing

if typing.TYPE_CHECKING:
    class Args(typing.Protocol):

        files: typing.List[pathlib.Path]
        validate: bool
        quiet: bool


def get_filename(element: et.Element) -> str:
    return element.attrib['filename']

def get_name(element: et.Element) -> str:
    return element.attrib['name']

def get_value(element: et.Element) -> int:
    return int(element.attrib['value'], 0)

def get_start(element: et.Element) -> int:
    return int(element.attrib['start'], 0)


BASE_TYPES = {
    'address',
    'offset',
    'int',
    'uint',
    'bool',
    'float',
    'mbz',
    'mbo',
}

FIXED_PATTERN = re.compile(r"(s|u)(\d+)\.(\d+)")

def is_base_type(name: str) -> bool:
    return name in BASE_TYPES or FIXED_PATTERN.match(name) is not None

def add_struct_refs(items: typing.OrderedDict[str, bool], node: et.Element) -> None:
    if node.tag == 'field':
        if 'type' in node.attrib and not is_base_type(node.attrib['type']):
            t = node.attrib['type']
            items[t] = True
        return
    if node.tag not in {'struct', 'group'}:
        return
    for c in node:
        add_struct_refs(items, c)


class Struct(object):
    def __init__(self, xml: et.Element):
        self.xml = xml
        self.name = xml.attrib['name']
        self.deps: typing.OrderedDict[str, Struct] = OrderedDict()

    def find_deps(self, struct_dict, enum_dict) -> None:
        deps: typing.OrderedDict[str, bool] = OrderedDict()
        add_struct_refs(deps, self.xml)
        for d in deps.keys():
            if d in struct_dict:
                self.deps[d] = struct_dict[d]
            else:
                assert d in enum_dict

    def add_xml(self, items: typing.OrderedDict[str, et.Element]) -> None:
        for d in self.deps.values():
            d.add_xml(items)
        items[self.name] = self.xml


# ordering of the various tag attributes
GENXML_DESC = {
    'genxml'      : [ 'name', 'gen', ],
    'import'      : [ 'name', ],
    'exclude'     : [ 'name', ],
    'enum'        : [ 'name', 'value', 'prefix', ],
    'struct'      : [ 'name', 'length', ],
    'field'       : [ 'name', 'start', 'end', 'type', 'default', 'prefix', 'nonzero' ],
    'instruction' : [ 'name', 'bias', 'length', 'engine', ],
    'value'       : [ 'name', 'value', 'dont_use', ],
    'group'       : [ 'count', 'start', 'size', ],
    'register'    : [ 'name', 'length', 'num', ],
}


def node_validator(old: et.Element, new: et.Element) -> bool:
    """Compare to ElementTree Element nodes.
    
    There is no builtin equality method, so calling `et.Element == et.Element` is
    equivalent to calling `et.Element is et.Element`. We instead want to compare
    that the contents are the same, including the order of children and attributes
    """
    return (
        # Check that the attributes are the same
        old.tag == new.tag and
        old.text == new.text and
        old.tail == new.tail and
        list(old.attrib.items()) == list(new.attrib.items()) and
        len(old) == len(new) and

        # check that there are no unexpected attributes
        set(new.attrib).issubset(GENXML_DESC[new.tag]) and

        # check that the attributes are sorted
        list(new.attrib) == list(old.attrib) and
        all(node_validator(f, s) for f, s in zip(old, new))
    )


def process_attribs(elem: et.Element) -> None:
    valid = GENXML_DESC[elem.tag]
    # sort and prune attributes
    elem.attrib = OrderedDict(sorted(((k, v) for k, v in elem.attrib.items() if k in valid),
                                     key=lambda x: valid.index(x[0])))
    for e in elem:
        process_attribs(e)


def sort_xml(xml: et.ElementTree) -> None:
    genxml = xml.getroot()

    imports = xml.findall('import')

    enums = sorted(xml.findall('enum'), key=get_name)
    enum_dict: typing.Dict[str, et.Element] = {}
    for e in enums:
        e[:] = sorted(e, key=get_value)
        enum_dict[e.attrib['name']] = e

    # Structs are a bit annoying because they can refer to each other. We sort
    # them alphabetically and then build a graph of dependencies. Finally we go
    # through the alphabetically sorted list and print out dependencies first.
    structs = sorted(xml.findall('./struct'), key=get_name)
    wrapped_struct_dict: typing.Dict[str, Struct] = {}
    for s in structs:
        s[:] = sorted(s, key=get_start)
        ws = Struct(s)
        wrapped_struct_dict[ws.name] = ws

    for ws in wrapped_struct_dict.values():
        ws.find_deps(wrapped_struct_dict, enum_dict)

    sorted_structs: typing.OrderedDict[str, et.Element] = OrderedDict()
    for s in structs:
        _s = wrapped_struct_dict[s.attrib['name']]
        _s.add_xml(sorted_structs)

    instructions = sorted(xml.findall('./instruction'), key=get_name)
    for i in instructions:
        i[:] = sorted(i, key=get_start)

    registers = sorted(xml.findall('./register'), key=get_name)
    for r in registers:
        r[:] = sorted(r, key=get_start)

    new_elems = (imports + enums + list(sorted_structs.values()) +
                 instructions + registers)
    for n in new_elems:
        process_attribs(n)
    genxml[:] = new_elems


class GenXml(object):
    def __init__(self, filename, import_xml=False, files=None):
        if files is not None:
            self.files = files
        else:
            self.files = set()
        self.filename = os.path.realpath(filename)
        assert self.filename not in self.files
        self.files.add(self.filename)
        self.et = et.parse(self.filename)
        if import_xml:
            self.merge_imported()

    def process_imported(self, merge=False, drop_dupes=False):
        assert merge != drop_dupes
        orig_elements = set(self.et.getroot())
        name_and_obj = lambda i: (get_name(i), i)
        filter_ty = lambda s: filter(lambda i: i.tag == s, orig_elements)
        filter_ty_item = lambda s: dict(map(name_and_obj, filter_ty(s)))
        orig_by_tag = {
            'enum': filter_ty_item('enum'),
            'struct': filter_ty_item('struct'),
            'instruction': filter_ty_item('instruction'),
            'register': filter_ty_item('register'),
        }
        for item in orig_elements:
            if item.tag == 'import':
                assert 'name' in item.attrib
                filename = os.path.split(item.attrib['name'])
                exceptions = set()
                for e in item:
                    assert e.tag == 'exclude'
                    exceptions.add(e.attrib['name'])
                # We should be careful to restrict loaded files to
                # those under the source or build trees. For now, only
                # allow siblings of the current xml file.
                assert filename[0] == '', 'Directories not allowed with import'
                filename = os.path.join(os.path.dirname(self.filename),
                                        filename[1])
                assert os.path.exists(filename), f'{self.filename} {filename}'
                genxml = GenXml(filename, import_xml=True, files=self.files)
                imported_elements = set(genxml.et.getroot())
                to_add = set()
                to_remove = set()
                for i in imported_elements:
                    if (i.tag not in orig_by_tag or
                        i.attrib['name'] in exceptions):
                        continue
                    if i.attrib['name'] in orig_by_tag[i.tag]:
                        if merge:
                            # When merging we ignore items that are
                            # present in the original genxml.
                            continue
                    else:
                        if drop_dupes:
                            # When droping duplicates we need to have
                            # items to compare that are present in
                            # both the original genxml and the
                            # imported genxml.
                            continue
                    if merge:
                        to_add.add(i)
                    else:
                        assert drop_dupes
                        orig_element = orig_by_tag[i.tag][i.attrib['name']]
                        if not node_validator(i, orig_element):
                            continue
                        to_remove.add(orig_element)
                if len(to_add) > 0:
                    assert len(to_remove) == 0
                    self.et.getroot().extend(list(to_add))
                    sort_xml(self.et)
                elif len(to_remove) > 0:
                    self.et.getroot()[:] = list(orig_elements - to_remove)
                    sort_xml(self.et)

    def merge_imported(self):
        self.process_imported(merge=True)

    def optimize_xml_import(self):
        self.process_imported(drop_dupes=True)

    def filter_engines(self, engines):
        changed = False
        items = []
        for item in self.et.getroot():
            # When an instruction doesn't have the engine specified,
            # it is considered to be for all engines. Otherwise, we
            # check to see if it's tagged for the engines requested.
            if item.tag == 'instruction' and 'engine' in item.attrib:
                i_engines = set(item.attrib["engine"].split('|'))
                if not (i_engines & engines):
                    # Drop this instruction because it doesn't support
                    # the requested engine types.
                    changed = True
                    continue
            items.append(item)
        if changed:
            self.et.getroot()[:] = items
