# Dictee

A simple minimalist text editor written in C heavily inspired by kilo

## Build

```bash
make update-submodules
make
```

## Usage

```bash
./dictee
# or with a filename
./dictee test.txt
```

## Debug

with gdb

```bash
gdb --args dictee test.txt
b 165
run
print somevariable
c continue
s step
```

redirect stderr to `error.log`

```bash
./dictee 2> error.log

```

## TODO

- include c-utils and use stuff from there

## Sources

<https://viewsourcecode.org/snaptoken/kilo/05.aTextEditor.html>
