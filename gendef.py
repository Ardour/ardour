from waflib.Task import Task
from waflib.TaskGen import feature, after_method
import re, subprocess

class dumpbin_def(Task):
    """Parses object files via dumpbin to generate linker .def files."""
    def run(self):
        exports = set()

        # Filters: skip compiler internals and validate section IDs
        blacklist = re.compile(r'__(?:real|x86\.get_pc|GLOBAL__|tls_|imp_|guard_|fstack|asan_|msan_|eh|unwind|purecall|CRT|vcrt|MSVC)')
        section_re = re.compile(r'\bSECT([1-9]\d*)\b')
        
        for obj in self.inputs:
            out = self.generator.bld.cmd_and_log(self.env.DUMPBIN + ['/SYMBOLS', obj.abspath()], quiet=True)
            
            for line in out.splitlines():
                # Relaxed filtering for non-debug builds
                if ' External ' not in line:
                    continue

                sym = None
                if '|' in line:
                    try:
                        sym = line.split('|')[1].strip().split()[0]
                    except (IndexError, ValueError):
                        sym = None
                
                # CRITICAL: Handle vtable/vbtable/vbase-dtor symbols BEFORE aggressive filtering
                # These use MSVC-specific prefixes and must be exported for DLLs with virtual inheritance
                if sym and any(sym.startswith(m) for m in ['??_7', '??_8', '??_D']):
                    # ??_D = vbase dtor (function), ??_7/??_8 = vftable/vbtable (data)
                    exports.add(sym if sym.startswith('??_D') else f'{sym} DATA')
                    continue
                
                # Filter: must be defined external symbol with valid section
                if any(x in line for x in ['UNDEF', 'Static']) or 'External' not in line: continue
                if not sym or blacklist.search(sym): continue
                
                if any(m in sym for m in ['??_E', '??_G']): continue
                if not (2 < len(sym) < 512) or (sym.startswith(('_Z', '?')) and self.is_internal_cpp(sym)): continue
                
                # Append DATA keyword for non-functions
                exports.add(f'{sym} DATA' if '()' not in line else sym)
        
        self.outputs[0].write('EXPORTS\n' + '\n'.join(f'    {s}' for s in sorted(exports)))

    def is_internal_cpp(self, sym):
        """Demangle check to skip RTTI and internal implementation details."""
        try:
            out = subprocess.check_output(self.env.UNDNAME + [sym], text=True, timeout=2)
            if any(m in out for m in ['`RTTI', '`type_info', 'internal::']): return True
            return False
        except Exception: return False

@feature('gendef')
@after_method('apply_link')
def add_def_task(self):
    if not self.env.DUMPBIN or not getattr(self, 'link_task', None): return
    
    # Extract object files from compiled tasks
    objs = [o for t in getattr(self, 'compiled_tasks', []) for o in t.outputs]
    if not objs: return
    
    def_node = self.path.find_or_declare(f"{self.target}.def")
    self.create_task('dumpbin_def', objs, def_node)
    
    self.link_task.dep_nodes.append(def_node)
    self.link_task.env.append_unique('LINKFLAGS', [f'/DEF:{def_node.abspath()}', '/DLL'])