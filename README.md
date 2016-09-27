**First readme**

cmake ..
make

Then at bin:

From one terminal (always first receiver, it starts a unix socket server):
./receiver

From other terminal:
./sender

sender will send symbols towards the receiver, wiht a 20% of PER and random ID.