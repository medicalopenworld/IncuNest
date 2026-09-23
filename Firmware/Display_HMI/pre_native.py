Import("env")
import os
env.PrependENVPath("PATH", "C:\\msys64\\mingw64\\bin")

# The native platform's LDF never compiles ../shared/src/*.cpp (headers are
# found via the -I../shared/include build flag, so no library dependency is
# registered; the symlink:// lib_deps entry alone was verified insufficient
# here). Compile shared/src explicitly for this test-only environment.
shared_src_dir = os.path.join(env["PROJECT_DIR"], "..", "shared", "src")
if os.path.isdir(shared_src_dir):
    env.Append(PIOBUILDFILES=env.BuildSources(
        os.path.join("$BUILD_DIR", "shared_lib"),
        shared_src_dir
    ))
