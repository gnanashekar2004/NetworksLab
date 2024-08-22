# Networks Assignment - 6
The project is a simple DNS server that provides IP addresses to clients who request them by sending domain names. It aims to facilitate the process of resolving domain names to their corresponding IP addresses.

## Team Members
- Kandula Revanth (21CS10035)
- Pola Gnana Shekar (21CS10052)

## Instruction to Run
clean all exisiting files, if any, using:
```bash
make clean
```

Compile the files using the command:
```bash
make all
```

Open a seperate terminal and run the following command to run the server in this terminal:
```bash
make run-server
```

Now open one more terminal to run client, use the command:
```bash
make run-client
```

## Examples
run the following in the client side terminal to test the functionality:
1. getIP 3 www.yahoo.com www.google.com www.youtube.com
2. getIP 2 example www.facebook.com
3. getIP 5 www.example www.google.com www.reddit.com www.amazon.com www.yahoo.com
4. getIP 9 www.yahoo.com www.google.com www.reddit.com www.amazon.com www.facebook.com user www.twitter.com www.netflix.com www.microsoft.com

## Note
- We are using sudo command to run exectables, otherwise it may throw permission denied error. This may ask you to enter your password.
- We are using "wlp2s0" as our interface it this doesn't work (shows up an error of No such device found) then you can replace with some other interface which you can find using ifconfig command
