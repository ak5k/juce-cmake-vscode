# JUCE VSCode CMake template
sets up:
* empty git repo
* juce as submodule
* simple juce plugin template
* configure.h.in
* vscode launch.json to start debug in daw, e.g. reaper
* vcpkg with vcpkg.json for easy deps management
* basic .gitignore
* see CMakeLists.txt


## Install
```
git clone https://github.com/ak5k/juce-vscode-cmake
```
Open with VSCode. Needs VSCode C/C++ and extensions.


or:

```
mkdir build
cd build

# new Windows e.g.
cmake -G "Visual Studio 17 2022" -A x64 ../
# Mac
cmake -G Xcode ../

you know the rest
```