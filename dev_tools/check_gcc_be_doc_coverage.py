#!/usr/bin/env python3
import os
import sys
import xml.etree.ElementTree as ET
import subprocess
import tempfile
import glob

def run_doxygen():
    # Write a temporary Doxyfile
    doxyfile_content = """
PROJECT_NAME           = "GCC Backend"
OUTPUT_DIRECTORY       = docs_tmp
GENERATE_XML           = YES
GENERATE_HTML          = NO
GENERATE_LATEX         = NO
INPUT                  = src lib_src tests/gcc_be
FILE_PATTERNS          = gcc_gen_be_*.h gcc_gen_be_*.c gcc_be_*.c gcc_be_*.cpp test_*.h test_*.c test_*.cpp
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = YES
EXTRACT_STATIC         = YES
MACRO_EXPANSION        = YES
EXPAND_ONLY_PREDEF     = YES
PREDEFINED             = BACK_END_IS_GCC_GEN_BE=1 GCC_GEN_BE_NODISCARD= GCC_GEN_BE_NOEXCEPT= BEGIN_EDG_NAMESPACE= END_EDG_NAMESPACE=
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_NO_PARAMDOC       = YES
"""
    with open("Doxyfile_doc_check", "w") as f:
        f.write(doxyfile_content)
    
    subprocess.run(["doxygen", "Doxyfile_doc_check"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.remove("Doxyfile_doc_check")

def has_text(element):
    if element is None:
        return False
    # Check if element has any non-placeholder text
    text = "".join(element.itertext()).strip()
    if "Missing brief" in text or "Missing details" in text:
        return False
    return len(text) > 0

def check_coverage():
    fail = False
    xml_dir = "docs_tmp/xml"
    if not os.path.exists(xml_dir):
        print("Doxygen XML output not found.")
        return True

    for xml_file in glob.glob(os.path.join(xml_dir, "*.xml")):
        if xml_file.endswith("index.xml"):
            continue
        try:
            tree = ET.parse(xml_file)
        except:
            continue
            
        root = tree.getroot()
        for compound in root.findall('compounddef'):
            kind = compound.get('kind')
            name = compound.findtext('compoundname')
            
            # Skip non-backend files
            if kind == 'file' and not (name.startswith('gcc_gen_be_') or name.startswith('gcc_be_') or name.startswith('test_')):
                continue
            # Skip non-backend structs/enums (though they are generated from these files)
            
            # File
            if kind == 'file':
                brief = compound.find('briefdescription')
                details = compound.find('detaileddescription')
                if not has_text(brief):
                    print(f"FAIL: File {name} is missing @brief.")
                    fail = True
                if not has_text(details):
                    print(f"FAIL: File {name} is missing @details.")
                    fail = True
            
            # Struct, Union, Enum
            if kind in ['struct', 'union', 'enum']:
                brief = compound.find('briefdescription')
                if not has_text(brief):
                    print(f"FAIL: {kind} {name} is missing @brief.")
                    fail = True
            
            # Members
            for member in compound.findall('.//memberdef'):
                mkind = member.get('kind')
                mname = member.findtext('name')
                
                # Check enum values, variables (fields), typedefs, and macro defines
                if mkind in ['enumvalue', 'variable', 'typedef', 'define']:
                    brief = member.find('briefdescription')
                    if not has_text(brief):
                        print(f"FAIL: {mkind} {mname} in {name} is missing @brief.")
                        fail = True
                
                # Check functions
                if mkind == 'function':
                    brief = member.find('briefdescription')
                    details = member.find('detaileddescription')
                    
                    if not has_text(brief):
                        print(f"FAIL: Function {mname} in {name} is missing @brief.")
                        fail = True
                    if not has_text(details):
                        print(f"FAIL: Function {mname} in {name} is missing @details.")
                        fail = True
                    
                    # Check return
                    type_str = "".join(member.find('type').itertext()).strip()
                    if type_str != 'void' and type_str != '':
                        ret = details.find('.//simplesect[@kind="return"]')
                        if ret is None or not has_text(ret):
                            print(f"FAIL: Function {mname} in {name} returning '{type_str}' is missing @return.")
                            fail = True
                            
                    # Check params
                    params = member.findall('param')
                    for param in params:
                        pname_elem = param.find('declname')
                        if pname_elem is None:
                            continue
                        pname = pname_elem.text
                        if pname == 'void':
                            continue
                        
                        # Find in detaileddescription
                        param_found = False
                        for paramlist in details.findall('.//parameterlist[@kind="param"]'):
                            for pitem in paramlist.findall('parameteritem'):
                                for pnamelist in pitem.findall('parameternamelist'):
                                    for pnm in pnamelist.findall('parametername'):
                                        if pnm.text == pname:
                                            param_found = True
                        if not param_found:
                            print(f"FAIL: Function {mname} in {name} is missing documentation for parameter '{pname}'.")
                            fail = True

    return fail

if __name__ == '__main__':
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(project_root)
    if not os.path.exists('src/gcc_gen_be_error.c'):
        print("Please run from project root")
        sys.exit(1)
        
    run_doxygen()
    fail = check_coverage()
    
    # cleanup
    import shutil
    if os.path.exists('docs_tmp'):
        shutil.rmtree('docs_tmp')
        
    if fail:
        print("Documentation coverage is incomplete.")
        sys.exit(1)
    else:
        print("100% Documentation Coverage verified.")
        sys.exit(0)
