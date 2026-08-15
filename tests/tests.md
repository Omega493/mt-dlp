# Tests

You can create files of various sizes on your PC and use them to test the utility out:

## Creating a Blank File

### On Linux

Use the following command:
```bash
dd if=/dev/zero of=<name> bs=<block_size> count=<count>
```

- `name`: This is the name of the file.
- `block_size`: This is the size of each block.
- `count`: This is the number of times to repeat the block size.

The final size of the file would be `block_size * count`.
Example, if the `block_size` was `1MB` and `count` was `10`, the file's size would be `10MB`.

Example usage:
```bash
# This will create a file of size 10MB.
dd if=/dev/zero of=10MB.bin bs=1MB count=10

# This will create a file of size 15MB.
dd if=/dev/zero of=15MB.bin bs=1MB count=15

# This will create a file of size 25MB.
dd if=/dev/zero of=25MB.bin bs=5MB count=5

# This will create a file of size 50MB.
dd if=/dev/zero of=50MB.bin bs=5MB count=10

# This will create a file of size 100MB.
dd if=/dev/zero of=100MB.bin bs=10MB count=10

# This will create a file of size 1GB.
dd if=/dev/zero of=1GB.bin bs=10MB count=100
```

## On Windows

Use the following command:
```cmd
fsutil file createnew <name> <size_in_bytes>
```

- `name`: The name of the file
- `size_in_bytes`: The size of the file in bytes.

Example usage:
```bash
# This will create a file of size 10MB.
fsutil file createnew 10MB.bin 10485760

# This will create a file of size 15MB.
fsutil file createnew 15MB.bin 15728640

# This will create a file of size 25MB.
fsutil file createnew 25MB.bin 26214400

# This will create a file of size 50MB.
fsutil file createnew 50MB.bin 52428800

# This will create a file of size 100MB.
fsutil file createnew 100MB.bin 104857600

# This will create a file of size 1GB.
fsutil file createnew 1GB.bin 1073741824
```

## Starting a Server

On both Windows and Linux, you can use the following to start a server. Run these in the same folder where you
created the files.
```bash
python -m http.server <port>

# Or,
python -m RangeHTTPServer <port>
```

- `port`: This specifies the port to where the server should listen to. It must be a number between `0` and `65535`.
However, for our use case, it is better if it was between `1024` and `49151`. `8000` and `8080` are two well-known ports.

You can now access the server at `http://<IP>:<port>`, where,
- `IP` is the IP Address of the server.
- `port` is the port number you specified above.

`http.server` doesn't support ranges, `RangeHTTPServer` does.

To install `RangeHTTPServer`, simply run
```bash
pip install rangehttpserver
```

Example usage:
```bash
python -m http.server 8080
python -m RangeHTTPServer 8080
```

If your server is located at some IP, say, `192.168.0.200`, then, you can access the folder with `http://192.168.0.200:8080`.

## Using the Utility

Now you can simply summon the utility using:
```bash
mt-dlp "http://<IP>:<port>/<filename>"
```

Example, if the IP is `192.168.0.200`, port is `8080` and file name is `100MB.bin`,
```bash
mt-dlp "http://192.168.0.200:8080/100MB.bin"
```
