# DeepStream application template

Copy this directory to `apps/<name>`, rename the executable, and add it to the root
CMake build behind a `BUILD_<NAME>` option. Keep domain code and SDK plugins in the
application. Depend only on the shared libraries that the application actually uses.
