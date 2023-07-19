# a text editor

https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

## Build

```bash
make
```

## Usage

```bash
./dictee
```

## Debug

```
gdb --args dictee test.txt
b 165
run
print somevariable
c continue
s step
```

## notes

peut etre plus interessant de ne pas traiter les rows separement mais avoir un seul buffer ?
