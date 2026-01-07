# Web Socket Echo Server Demo
Web Socket Echo Server Demo using Libwebsocket library for FRDM-IMX91 development board.

## Compile
Press <b>Ctrl+Shift+B</b><br>
In order to compile and upload executable to targer, you need to change the next variable values in <b>settings.json</b> in <b>.vscode</b> folder:

- myRemoteIpAddr
- myRemoteFolder

## Debug
Press <b>F5</b><br>
In order to debug you need to start <b>gdb server</b>:

- gdbserver :2345 ./executable


## Repo structre

```text
├── .vscode/                    # VS Code config folder
├── include/                    # Program libraries
│   ├── cjson/                  # JSON handler library
│   ├── custom/                 # Custom libraries
│   ├── libuv/                  # Libuv library
│   ├── websockets/             # libwebsockets library
│   └── ws_raw/                 # Web Socket API library
│
├── lib/                        # .so / .a compiled libraries
│
├── benchmark_node              # A simple websocket benchmark using Node
├── main.c                      # Main application
└── readme.md                   # Project documentation
```

## Run in target
In order to run in target, you need in the same folder the next files:

- libuv.so
- libuv.so.1
- libuv.so.1.0.0
- libwebsockets-evlib_uv.so
- libwebsockets.so
- policy.json
- main (Executable)cd li
  
And use this command: 

```cmd
LD_LIBRARY_PATH=.:/usr/lib:/lib ./main
```
