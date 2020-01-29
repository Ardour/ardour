#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2010 (ita)

"""
This tool is totally deprecated

Try using:
	.pc.in files for .pc files
	the feature intltool_in - see demos/intltool
	make-like rules
"""

import shutil, re, os
from waflib import TaskGen, Node, Task, Utils, Build, Errors
from waflib.TaskGen import feature, after_method, before_method
from waflib.Logs import debug

def copy_attrs(orig, dest, names, only_if_set=False):
	"""
	copy class attributes from an object to another
	"""
	for a in Utils.to_list(names):
		u = getattr(orig, a, ())
		if u or not only_if_set:
			setattr(dest, a, u)

def copy_func(tsk):
	"Make a file copy. This might be used to make other kinds of file processing (even calling a compiler is possible)"
	env = tsk.env
	infile = tsk.inputs[0].abspath()
	outfile = tsk.outputs[0].abspath()
	try:
		shutil.copy2(infile, outfile)
	except (OSError, IOError):
		return 1
	else:
		if tsk.chmod: os.chmod(outfile, tsk.chmod)
		return 0

def action_process_file_func(tsk):
	"Ask the function attached to the task to process it"
	if not tsk.fun: raise Errors.WafError('task must have a function attached to it for copy_func to work!')
	return tsk.fun(tsk)

@feature('cmd')
def apply_cmd(self):
	"call a command everytime"
	if not self.fun: raise Errors.WafError('cmdobj needs a function!')
	tsk = Task.TaskBase()
	tsk.fun = self.fun
	tsk.env = self.env
	self.tasks.append(tsk)
	tsk.install_path = self.install_path

@feature('copy')
@before_method('process_source')
def apply_copy(self):
	Utils.def_attrs(self, fun=copy_func)
	self.default_install_path = 0

	lst = self.to_list(self.source)
	self.meths.remove('process_source')

	for filename in lst:
		node = self.path.find_resource(filename)
		if not node: raise Errors.WafError('cannot find input file %s for processing' % filename)

		target = self.target
		if not target or len(lst)>1: target = node.name

		# TODO the file path may be incorrect
		newnode = self.path.find_or_declare(target)

		tsk = self.create_task('copy', node, newnode)
		tsk.fun = self.fun
		tsk.chmod = getattr(self, 'chmod', Utils.O644)

		if not tsk.env:
			tsk.debug()
			raise Errors.WafError('task without an environment')

####################
## command-output ####
####################

class cmd_arg(object):
	"""command-output arguments for representing files or folders"""
	def __init__(self, name, template='%s'):
		self.name = name
		self.template = template
		self.node = None

class input_file(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_resource(self.name)
		if self.node is None:
			raise Errors.WafError("Input file %s not found in " % (self.name, base_path))

	def get_path(self, env, absolute):
		if absolute:
			return self.template % self.node.abspath()
		else:
			return self.template % self.node.srcpath()

class output_file(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_or_declare(self.name)
		if self.node is None:
			raise Errors.WafError("Output file %s not found in " % (self.name, base_path))

	def get_path(self, env, absolute):
		if absolute:
			return self.template % self.node.abspath()
		else:
			return self.template % self.node.bldpath()

class cmd_dir_arg(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_dir(self.name)
		if self.node is None:
			raise Errors.WafError("Directory %s not found in " % (self.name, base_path))

class input_dir(cmd_dir_arg):
	def get_path(self, dummy_env, dummy_absolute):
		return self.template % self.node.abspath()

class output_dir(cmd_dir_arg):
	def get_path(self, env, dummy_absolute):
		return self.template % self.node.abspath()


class command_output(Task.Task):
	color = "BLUE"
	def __init__(self, env, command, command_node, command_args, stdin, stdout, cwd, os_env, stderr):
		Task.Task.__init__(self, env=env)
		assert isinstance(command, (str, Node.Node))
		self.command = command
		self.command_args = command_args
		self.stdin = stdin
		self.stdout = stdout
		self.cwd = cwd
		self.os_env = os_env
		self.stderr = stderr

		if command_node is not None: self.dep_nodes = [command_node]
		self.dep_vars = [] # additional environment variables to look

	def run(self):
		task = self
		#assert len(task.inputs) > 0

		def input_path(node, template):
			if task.cwd is None:
				return template % node.bldpath()
			else:
				return template % node.abspath()
		def output_path(node, template):
			fun = node.abspath
			if task.cwd is None: fun = node.bldpath
			return template % fun()

		if isinstance(task.command, Node.Node):
			argv = [input_path(task.command, '%s')]
		else:
			argv = [task.command]

		for arg in task.command_args:
			if isinstance(arg, str):
				argv.append(arg)
			else:
				assert isinstance(arg, cmd_arg)
				argv.append(arg.get_path(task.env, (task.cwd is not None)))

		if task.stdin:
			stdin = open(input_path(task.stdin, '%s'))
		else:
			stdin = None

		if task.stdout:
			stdout = open(output_path(task.stdout, '%s'), "w")
		else:
			stdout = None

		if task.stderr:
			stderr = open(output_path(task.stderr, '%s'), "w")
		else:
			stderr = None

		if task.cwd is None:
			cwd = ('None (actually %r)' % os.getcwd())
		else:
			cwd = repr(task.cwd)
		debug("command-output: cwd=%s, stdin=%r, stdout=%r, argv=%r" %
			     (cwd, stdin, stdout, argv))

		if task.os_env is None:
			os_env = os.environ
		else:
			os_env = task.os_env
		command = Utils.subprocess.Popen(argv, stdin=stdin, stdout=stdout, stderr=stderr, cwd=task.cwd, env=os_env)
		return command.wait()

@feature('command-output')
def init_cmd_output(self):
	Utils.def_attrs(self,
		stdin = None,
		stdout = None,
		stderr = None,
		# the command to execute
		command = None,

		# whether it is an external command; otherwise it is assumed
		# to be an executable binary or script that lives in the
		# source or build tree.
		command_is_external = False,

		# extra parameters (argv) to pass to the command (excluding
		# the command itself)
		argv = [],

		# dependencies to other objects -> this is probably not what you want (ita)
		# values must be 'task_gen' instances (not names!)
		dependencies = [],

		# dependencies on env variable contents
		dep_vars = [],

		# input files that are implicit, i.e. they are not
		# stdin, nor are they mentioned explicitly in argv
		hidden_inputs = [],

		# output files that are implicit, i.e. they are not
		# stdout, nor are they mentioned explicitly in argv
		hidden_outputs = [],

		# change the subprocess to this cwd (must use obj.input_dir() or output_dir() here)
		cwd = None,

		# OS environment variables to pass to the subprocess
		# if None, use the default environment variables unchanged
		os_env = None)

@feature('command-output')
@after_method('init_cmd_output')
def apply_cmd_output(self):
	if self.command is None:
		raise Errors.WafError("command-output missing command")
	if self.command_is_external:
		cmd = self.command
		cmd_node = None
	else:
		cmd_node = self.path.find_resource(self.command)
		assert cmd_node is not None, ('''Could not find command '%s' in source tree.
Hint: if this is an external command,
use command_is_external=True''') % (self.command,)
		cmd = cmd_node

	if self.cwd is None:
		cwd = None
	else:
		assert isinstance(cwd, CmdDirArg)
		self.cwd.find_node(self.path)

	args = []
	inputs = []
	outputs = []

	for arg in self.argv:
		if isinstance(arg, cmd_arg):
			arg.find_node(self.path)
			if isinstance(arg, input_file):
				inputs.append(arg.node)
			if isinstance(arg, output_file):
				outputs.append(arg.node)

	if self.stdout is None:
		stdout = None
	else:
		assert isinstance(self.stdout, str)
		stdout = self.path.find_or_declare(self.stdout)
		if stdout is None:
			raise Errors.WafError("File %s not found" % (self.stdout,))
		outputs.append(stdout)

	if self.stderr is None:
		stderr = None
	else:
		assert isinstance(self.stderr, str)
		stderr = self.path.find_or_declare(self.stderr)
		if stderr is None:
			raise Errors.WafError("File %s not found" % (self.stderr,))
		outputs.append(stderr)

	if self.stdin is None:
		stdin = None
	else:
		assert isinstance(self.stdin, str)
		stdin = self.path.find_resource(self.stdin)
		if stdin is None:
			raise Errors.WafError("File %s not found" % (self.stdin,))
		inputs.append(stdin)

	for hidden_input in self.to_list(self.hidden_inputs):
		node = self.path.find_resource(hidden_input)
		if node is None:
			raise Errors.WafError("File %s not found in dir %s" % (hidden_input, self.path))
		inputs.append(node)

	for hidden_output in self.to_list(self.hidden_outputs):
		node = self.path.find_or_declare(hidden_output)
		if node is None:
			raise Errors.WafError("File %s not found in dir %s" % (hidden_output, self.path))
		outputs.append(node)

	if not (inputs or getattr(self, 'no_inputs', None)):
		raise Errors.WafError('command-output objects must have at least one input file or give self.no_inputs')
	if not (outputs or getattr(self, 'no_outputs', None)):
		raise Errors.WafError('command-output objects must have at least one output file or give self.no_outputs')

	cwd = self.bld.variant_dir
	task = command_output(self.env, cmd, cmd_node, self.argv, stdin, stdout, cwd, self.os_env, stderr)
	task.generator = self
	copy_attrs(self, task, 'before after ext_in ext_out', only_if_set=True)
	self.tasks.append(task)

	task.inputs = inputs
	task.outputs = outputs
	task.dep_vars = self.to_list(self.dep_vars)

	for dep in self.dependencies:
		assert dep is not self
		dep.post()
		for dep_task in dep.tasks:
			task.set_run_after(dep_task)

	if not task.inputs:
		# the case for svnversion, always run, and update the output nodes
		task.runnable_status = type(Task.TaskBase.run)(runnable_status, task, task.__class__) # always run
		task.post_run = type(Task.TaskBase.run)(post_run, task, task.__class__)

	# TODO the case with no outputs?

def post_run(self):
	for x in self.outputs:
		x.sig = Utils.h_file(x.abspath())

def runnable_status(self):
	return self.RUN_ME

Task.task_factory('copy', vars=[], func=action_process_file_func)

