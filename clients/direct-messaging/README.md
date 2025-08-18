# Direct Messaging Clients

This directory contains two types of direct messaging implementations using the ur-rpc-template request-response standards:

## Standard Request-Response Messaging
- **client_requester.c**: Initiates RPC requests to the responder client
- **client_responder.c**: Responds to RPC requests from the requester client
- **requester_config.json**: Configuration for the requester client
- **responder_config.json**: Configuration for the responder client

## Queued Direct Messaging
- **queued_client_1.c**: Sends sequential requests (1, 2, 3...) and waits for each response
- **queued_client_2.c**: Processes requests in order and sends responses
- **queued_client_1_config.json**: Configuration for the first queued client
- **queued_client_2_config.json**: Configuration for the second queued client

## Build Instructions
```bash
make all
```

## Usage
For standard request-response:
```bash
./build/client_responder responder_config.json &
./build/client_requester requester_config.json
```

For queued messaging:
```bash
./build/queued_client_2 queued_client_2_config.json &
./build/queued_client_1 queued_client_1_config.json
```