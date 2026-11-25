Import("env")
# PlatformIO only passes build_flags to the compiler, not the linker.
# --coverage (which links -lgcov) must be added to LINKFLAGS explicitly.
env.Append(LINKFLAGS=["--coverage"])
