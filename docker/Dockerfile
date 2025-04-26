### Build/test container ###
# Define builder stage
FROM lebron2016x4:base as builder

# Share work directory
COPY . /usr/src/project
WORKDIR /usr/src/project/build

# Build and test
RUN cmake ..
RUN make
RUN ctest --output-on_failure


### Deploy container ###
# Define deploy stage
FROM ubuntu:noble as deploy

COPY --from=builder /usr/src/project/build/bin/webserver .
COPY --from=builder /usr/src/project/cloud_config .
COPY --from=builder /usr/src/project/www .

EXPOSE 80

# Use ENTRYPOINT to specify the binary name
ENTRYPOINT ["./webserver"]

# Use CMD to specify arguments to ENTRYPOINT
CMD ["cloud_config"]
