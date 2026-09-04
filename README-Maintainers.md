## Maintainer Document

The following document is for maintainers to read/understand/use to troubleshoot issues and
document them here.

### Testing the repository

To run the tests you have to build the entire toolchain, which includes the base LLVM,
compiler-rt, and patmos-newlib.

By default when running within your machine, it runs the truncated smoke-tests of
computationally heavy tests. If you want to run heavy tests, then add the
`PATMOS_RUN_EXPENSIVE_TESTS=1` environment variable.

The parallelisation of `-j4` is low and high enough to starve out your machine. Edit it 
higher at your own risk, lower is sometimes even safer thus faster.

To test LLVM  (from the `build` folder):

```
make -j4 llc UnitTests && ./bin/llvm-lit ../llvm/test -v --filter=Patmos
```

To test Clang:

```
make -j4 ClangPatmosTestDeps && ./bin/llvm-lit ../clang/test -v --filter=Patmos
```

To test LLD:

```
make -j4 lld && ./bin/llvm-lit ../lld/test -v --filter=Patmos
```

To test Compiler-RT, go to the `build-compiler-rt` folder:

```
./bin/llvm-lit -v test/builtins/Unit/patmos
```

----

### Packaging

To create a tarball containing the built compiler and standard library, use the following command from the `build` folder:

```
make PatmosPackage
```

To do this, you must have built LLVM, Compiler-RT, and `newlib` using the previous steps.
The tarball is then available under `build/patmos-unknown-unknown-elf/package-temp/patmos-llvm-*.tar.gz`, where `*` depends on the platform you are building on.
Using this tarball, you can install the compiler as described in the installation section.

_Note: The steps for building LLVM are meant for development and as such produce a debug-mode compiler (which is extremely slow).
To package a release compiler, use the flag `-DCMAKE_BUILD_TYPE=Release`_

### Releasing New Versions

To fully automatically publish a new version, simply tag the required commit using `x.y.z` version numbering (with an optional `-` followed by anything).
Github Actions will then automatically test the commit, make packages for each platform, and publish them all.

### <a name="anch-updating-llvm"></a>Updating LLVM

Even though this repository is a fork of [the official LLVM repository](https://github.com/llvm/llvm-project),
we only pull the upstream changes at official release tags.
The `upstream` branch is specifically intended for this and nothing else.
Its `HEAD` should always be a commit for an official release.
E.g. it could be tracking the commit for the 11.1.0 release.

Updating to a new release of upstream LLVM is not a trivial task.
The main difficulty comes from the fact that LLVM uses release branches, whic means their history splits after a major release.
This means trying to simply pull in the latest release tag will likely cause many merge conflicts in LLVM code that we (the Patmos project) haven't touched.
Trying to fix the merge conflict can be tedious and error prone.
Instead, the following guide will mimick mergin by simply applying Patmos' code directly to the new release.
This will cause much fewer and simply conflicts and is much less likely to be done incorrectly:

_`llvm-upstream` refers to the LLVM github repo: github.com/llvm/llvm-project._

_`t-crest` refers to the Patmos LLVM github repo: github.com/t-crest/patmos-llvm-project._

_As an example, `llvmorg-11.1.0` is the current LLVM version tag used by Patmos, and `llvmorg-12.0.1` is the one we are trying to update to. Change them as appropriate_

- Get the most recent LLVM release's tag:

```sh
git checkout master
git pull llvm-upstream --tags
```

- Update the `upstream` branch to point to it:

```sh
git branch -f upstream llvmorg-12.0.1
```

- Push the new tag and "upstream" to the t-crest repo:

```sh
git checkout upstream
git push t-crest +upstream
git push llvmorg-12.0.1
```

- Create a diff of the difference between Patmos HEAD and `llvmorg-11.1.0`, which will then contain all Patmos-specific code/changes:

```sh
git diff llvmorg-11.1.0 master > patmos-diff.patch
```

- Move to a temporary branch where you apply the changes to the latest LLVM release:

```sh
git checkout llvmorg-12.0.1 -b tempbranch
git apply --reject --verbose --ignore-whitespace patmos-diff.patch
```

- Git has now applied all changes it can. Where unable, it left a `*.rej` file containing the unapplied patmos changes (where `*` is th name of the file containing the unapplied code).
  Go through the project and apply rejected code manually and deleting the `*.rej` files as you go.
  To check whether you applied the diff correctly, you can print out the changes (`git diff > post-apply-patmos-diff.patch`) and compare it with the original diff (`diff post-apply-patmos-diff.patch patmos-diff.patch`).
  The differences should be minor and only in non-patmos code.

- Commit: `git commit -m "Merged Patmos HEAD with llvmorg-12.0.1"`

- Rewrite git history, to show that the last Patmos commit is also a parent of the new one (to make it look like we merged it with `llvmorg-12.0.1`):

```sh
git replace --graft tempbranch llvmorg-12.0.1 master
git filter-branch llvmorg-12.0.1..tempbranch
```

- Change `master` to point to the new commit, delete the temporary branch, and push to T-CREST reposity:

```sh
git branch -f master tempbranch
git checkout master
git branch -D tempbranch
git push origin +master
```

You have now effectively merged the old Patmos head with the new LLVM release tag.
The git history will showing as being a merge of the two commits.
After this, the Patmos code will probably need tweaking to conform to any changes LLVM has made since the last release.


----

### Platin Setup

Is it still complaining about lp_solve?

```zsh
brew tap brewsci/science
brew install lp_solve
```

How to initiate the install setup from the local directory?

```zsh
./patmos-platin/install.sh -i ./platin-inst
```

----

### How to do make Nix derivation less sensitive to changes in the source tree

```nix
let
  repoSrc = ../..;

  # Keep source hashing stable by excluding local build outputs.
  filteredRepoSrc = pkgs.lib.cleanSourceWith {
    src = repoSrc;
    filter = path: _type:
      let
        relPath = pkgs.lib.removePrefix "${toString repoSrc}/" (toString path);
        ignored =
          relPath == ".git"
          || pkgs.lib.hasPrefix ".git/" relPath
          || relPath == "build"
          || pkgs.lib.hasPrefix "build/" relPath
          || relPath == "build-compiler-rt"
          || pkgs.lib.hasPrefix "build-compiler-rt/" relPath
          || relPath == "build-newlib"
          || pkgs.lib.hasPrefix "build-newlib/" relPath
          || relPath == "result"
          || pkgs.lib.hasPrefix "result/" relPath
          || pkgs.lib.hasSuffix ".log" relPath;
      in
        !ignored;
  };
in
  filteredRepoSrc
```

---

### How to upload it to cachix manually

```zsh
nix build .#patmos-prefixed -L --show-trace --no-link --print-out-paths | cachix push kodalem
# Do not forget to copy the signing key, because they do not support multiple keys in multiple caches.
```

Maintenance - be sure to update the secrets in the github repository if a new cache signing key from cachix, and check the CI file as well in the workflow

---

### Extra flags

`-mpatmos-skip-postra-scheduler` is possible to use to skip Patmos post-RA scheduling to avoid reg-unit assertions. Default is `false`
but for whatever reason you might need it, there's an option for that. This was initially used to analyze crash issues during compiling to
eliminate red-herrings.

---

### Investigations for future maintainers

- `/llvm/test/Bindings/Go/lit.local.cfg` was removed in 2022:
  https://discourse.llvm.org/t/rfc-remove-the-go-bindings/65725
  - Possible replacement: https://github.com/tinygo-org/go-llvm

- `handleLoopboundAttr` in `Sema/SemaAttr.cpp`
  - Confirm ownership and whether it still belongs in that file.

- Current migration reference:
  - Use the RISCV target as a reference for how the system is wired together.
  - Identify what needs to be migrated into Patmos.
  - Check the corresponding LLVM implementation as part of that work.


----

### TableGen

If instruction header files are missing when using the IDE with CMake support,
regenerate the required TableGen outputs first.

```zsh
# Ensure tblgen is built
make -C build llvm-tblgen

# Generate Patmos instruction info
build/bin/llvm-tblgen -gen-instr-info -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenInstrInfo.inc

# Generate register info
make -C build llvm-tblgen && build/bin/llvm-tblgen -gen-register-info -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenRegisterInfo.inc
cp build/lib/Target/Patmos/PatmosGenRegisterInfo* llvm/lib/Target/Patmos/

# Generate asm matcher
build/bin/llvm-tblgen -gen-asm-matcher -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenAsmMatcher.inc
cp build/lib/Target/Patmos/PatmosGenAsmMatcher.inc llvm/lib/Target/Patmos/

# Generate calling convention info
build/bin/llvm-tblgen -gen-callingconv -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenCallingConv.inc
cp build/lib/Target/Patmos/PatmosGenCallingConv.inc llvm/lib/Target/Patmos/
```

----

### Debugging Patmos tests

```zsh
# 0) Find the first real blocker
./build/bin/llvm-lit llvm/test/CodeGen/Patmos/ -j1 -sv --max-failures=1

# 1) Reproduce that test directly
./build/bin/llvm-lit llvm/test/CodeGen/Patmos/<failing-test>.ll -sv

# 2) Rebuild the needed tool quickly
cmake --build build --target llc -j16

# 3) Re-run the first-failure scan
./build/bin/llvm-lit llvm/test/CodeGen/Patmos/ -j1 -sv --max-failures=1

# 4) Manually debug the command
# Check the .ll file for the exact flags.
./build/bin/llc -march=patmos < ./llvm/test/CodeGen/Patmos/flags/mpatmos-subfunction-align/align_multiple_subfunctions.ll
```

----

### Compiling and inspecting output

```zsh
# Compile the code
./build/bin/clang++ --target=patmos-unknown-unknown-elf -O2 -c ./fibonacci-test.cpp -o ./fibonacci-test.o

# Dump the object code
./build/bin/llvm-objdump -d ./fibonacci-test.o

# Generate the .pml file for Platin
# Important: use -mserialize-pml, not -mserialize
./build/bin/clang++ --target=patmos-unknown-unknown-elf -O2 -mserialize-pml=fibonacci.pml -c ./fibonacci-test.cpp -o fibonacci.out

# Run Platin
./.platin-inst/bin/platin wcet -i fibonacci.pml -b fibonacci.out -e main --report --disable-ait --objdump-command ./build/bin/llvm-objdump
```

----

### Newlib workflow with Nix

```zsh
# Build it
nix build .#patmos-newlib -L
# Copy it
nix copy --to file:///Volumes/T7/nix-cache .#patmos-newlib
# Collect it
nix-collect-garbage -d
# Restore it
nix copy --from file:///Volumes/T7/nix-cache .#patmos-newlib
# Break it *enter Daft Punk riff here*
```


