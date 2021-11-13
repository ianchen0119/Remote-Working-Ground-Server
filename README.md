# Remote Working Ground (rwg) Server

## features

- [x] remote shell
	- numbered pipe
	- multi pipe
	- environment variables
	- command execution
- [x] chat room
- [x] user pipe

## Intro

### user pipe

send the data to the other user:
```sh
ls >2 // send to user #2
```
receive the data from specified user:
```sh
cat <1 // receive from user #1
```

### chatroom

- who
- name
set a new name!
```
name Ian
```
- tell
```
tell 2 Hello! // tell to user #2
```
- yell
```
yell Hello, everyone! // broadcast
```
- exit

### numbered pipe

Try it!
```sh
ls |2
noop
cat
```