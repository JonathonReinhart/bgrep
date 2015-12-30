env = Environment(
    CCFLAGS = ['-Wall', '-Werror'],
)

conf = env.Configure()
if conf.CheckFunc('mmap'):
    conf.env.Append(CPPDEFINES = ['HAS_MMAP'])
env = conf.Finish()

if int(ARGUMENTS.get('coverage', 0)):
    env.Append(
        CCFLAGS = ['-coverage'],
        LINKFLAGS = ['-coverage'],
    )

env.Program('bgrep', ['bgrep.c'])
