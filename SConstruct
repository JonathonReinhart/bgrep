env = Environment(
    CCFLAGS = ['-Wall', '-Werror'],
)

if int(ARGUMENTS.get('coverage', 0)):
    env.Append(
        CCFLAGS = ['-coverage'],
        LINKFLAGS = ['-coverage'],
    )

env.Program('bgrep', ['bgrep.c'])
