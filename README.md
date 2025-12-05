Secure Stream Echo Server Demo using Libwebsocket library for FRDM-IMX91 development board

# Compile
Press <b>Ctrl+Shift+B</b><br>
In order to compile and upload executable to targer, you need to change the next variable values in <b>settings.json</b> in <b>.vscode</b> folder:

- myRemoteIpAddr
- myRemoteFolder

# Debug
Press <b>F5</b><br>
In order to debug you need to start <b>gdb server</b>:

- gdbserver :2345 ./executable


# Repo structre

```text
├── .vscode/                    # VS Code config folder
├── include/                    # Program libraries
│   ├── custom/                 # Custom libraries
│   └── websockets/             # libwebsockets library
│
├── lib/                        # .so / .a compiled libraries
│
├── web_socket_node_client      # A simple websocket client connection using Node
├── main.c                      # Main application
├── policy.json                 # Secure Streams policy
└── readme.md                   # Project documentation
```

# Run in target
In order to run in target, you need in the same folder the next files:

- libwebsockets.so
- policy.json
- main (Executable)