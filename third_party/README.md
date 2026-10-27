# Dependency notices

`components.tsv` records the resolved runtime components and the compile-only libxposed API. The Gradle task compares the table with the dependency graph before building and bundles the referenced license texts.

`app/src/main/assets/LIBCXX-NOTICE.txt` is the unmodified `NOTICE.toolchain` supplied with Android NDK 30.0.16248370. It includes the notices for the statically linked LLVM libc++ runtime. It is deliberately retained verbatim.
