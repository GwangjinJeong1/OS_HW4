# OS_HW4

## Build
```c
gcc -pthread -o findeq findeq.c
```
## Run the program
```c
./findeq <OPTION> DIR
```

>options:<br> 
> -t=NUM  creates upto NUM threads addition to the main thread. The give number is no more than 64.<br> 
> -m=NUM  ignores all files whose size is less than NUM bytes from the search. The default value is 1024.<br> 
> -o=FILE  produces to the output to FILE. By default, the output must be printed to the standard output.<br> 

## Example
```c
./findeq ./Files
```

```c
./findeq -t=8 -m=2048 ./Files
```

```c
./findeq -o=result ./Files
```
