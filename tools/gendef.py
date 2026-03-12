import re
import subprocess
from waflib.Task import Task
from waflib.TaskGen import feature, after_method

# inside your loop:

class nm_def(Task):
    p = re.compile(r'^\s*([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)')  # Parse llvm-nm output (grouped as address, symbol-type letter, symbol-name)
    s = re.compile(r'(?!__?imp_)[^?]*$|\?(?!\?_(?:[GE]|C@_))')  # Filters C/C++ symbols
    def __str__(self): return self.outputs[0].name              # Change what's said in the build log
    def run(self):
        p, s, nm = self.p.match, self.s.match, self.env.llvm_nm
        internal = re.compile(r'__(?:real|x86\.get_pc|GLOBAL__|tls_|imp_|guard_|fstack|asan_|msan_|eh|unwind|purecall|CRT|vcrt|MSVC)')
        self.outputs[0].write('EXPORTS\n' + '\n'.join(
            r
            for f in self.inputs                                        # For .o files
            for l in subprocess.check_output(nm + ['--defined-only', f.abspath()], text=1).splitlines() #Output of the llvm command run on the file
            if (m := p(l))                                              # Skip anything that isn't a symbol 
            and m[2].isupper()                                          # Uppercase letter means globally visible. Skip lowercase.
            and s(m[3])                                                 # Rejects import thunks (_imp_ prefix), C++ destructors, and string literals
            and (name := m[3])                                          # Define "name" from the result, for more filtering
            and name not in {"_local_stdio_scanf_options", "vfscanf_l", "vsscanf_l"}
            and not internal.search(name)                               # Filter out the internal symbols, and 3 others giving trouble.
            and (r := f'    {name}{" DATA"*(m[2].upper() in "BDRSV")}') # Format the export line; data symbols (B/D/R/S/V types) get a trailing "DATA".
        ))

@feature('gendef')
@after_method('apply_link')
def add_def(self):
    lt = self.link_task
    if not self.env.llvm_nm:
        return  # Return if llvm-nm isn't found

    obj_files = [f for f in lt.inputs if f.suffix() == '.o']
    d = self.path.find_or_declare(f'{self.target}.def') # Find/Declare the .def output path in the build directory
    self.create_task('nm_def', obj_files, d)            # Create the task
    lt.dep_nodes.append(d)                              # Tell the linker to wait for the .def file before linking
    lt.env.append_unique('LINKFLAGS', [f'/DEF:{d.abspath()}','/DLL']) # Pass the .def file to the linker, with /DLL.