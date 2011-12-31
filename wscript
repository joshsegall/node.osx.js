srcdir = '.'
blddir = 'build'

def set_options(opt):
  opt.tool_options('compiler_cxx')

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')
  conf.env.append_unique('CXXFLAGS', ['-Wall'])

def build(bld):
  obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
  obj.cxxflags = ["-Wall"]
  obj.target = 'osx'
  obj.source = 'osx.cc'
