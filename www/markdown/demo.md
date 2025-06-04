# Project Structure
**Basic Overview:**

```
lebron2016x4/
├── cmake/               # CMake configuration helpers (e.g., coverage macros)
├── docker/              # Dockerfiles for base image, app image, and coverage
├── include/             # Header files for all server modules and handlers
├── logs/                # (Optional) runtime logs for debugging/server output
├── src/                 # Source (.cc) files for server core and handlers
├── tests/               # Unit and integration tests (GoogleTest + Python)
├── www/                 # Files to serve via StaticHandler (e.g., HTML, CSS)
│ \
├── CMakeLists.txt       # Build system root config
├── my_config            # Local development config file
├── cloud_config         # Cloud production config file
├── .dockerignore        # Ignore rules for Docker context
├── .gitignore           # Git ignore rules
```

Additional directories you'll create during development:
- Note: These two directories are used to testing and should always be created in the root directory. Only within these directories should you be running any build commands.
```
build/                  # Used to compile and run binaries (standard CMake out-of-source)
build_coverage/         # Same as build/, but runs tests with coverage instrumentation
```

> ❌ Never build in the project root. CMake has a strict-in source build guard

## How to Create Directories for build and build coverage:

In the CLI, run the following commands:

**build**

``` bash
    rm -r build                 # If an old build directory already exists with stale binaries, remove it
    mkdir -p build              # Create a new build directory from the root folder
    cd build                    # cd into the build directory in order to build locally
```

**build_coverage**

``` bash
    rm -r build_coverage        # Same as above
    mkdir -p build_coverage
    cd build_coverage
```

# Nginx Config Overview
Our server is configured using a simplified Nginx config file. A typical config defines:
- The server port
- A list of location blocks, each bound to a handler
- Optional arguments passed to those handlers

**Example Config File:**

``` Nginx
# Configure port and routes
port 80;

#Route paths must be ordered(most to least specific) due to longest-prefix matching

location /echo EchoHandler {
}

location /static StaticHandler {
    root ./var/www/static;
}

location /public StaticHandler {
    root ./var/www/static;
}

location / NotFoundHandler {
}

```
> **⚠️ Route paths must be ordered(most to least specific) due to longest-prefix matching**

## Syntax Rules and Guidelines

### Defining a port in the config:
Always declare the port at the top of the config:
``` Nginx
port port_number;

-- rest of config
```
- The keyword "port" should always be followed by a valid port number followed by a semicolon. This must always be the first statement in a config.

### Adding Locations and Handlers in the config:
Each location block specifies a URL route and maps it to a handler:

**Cloud Config Example:**
``` Nginx
location /my_handler MyHandler {
    root ./var/path/to/files;
}
```
**Local Config Example:**
``` Nginx
location /my_handler MyHandler {
    root ../../path/to/files;
}
```
- The first argument '/my_handler' is the serving path and must start with /
- The second MyHandler is the name of the handler. This must match with the defined kName in the new handler's source code. For more on that, visit the section "How to Add a Request Handler."
- Arguments inside {...} are passed as std::unordered_map<std::string, std::string> to the handler factory that is in charge of dynamically instantiating handlers at runtime.

## Local vs Cloud Config Paths
The value of the root field in our config file depends on where the webserver binary is run from, and where your static files are located relative to that.

**Local Config Example:**
``` Nginx
location /static StaticHandler {
    root ../../www/static;
}
```
- The binary is run from: /usr/src/projects/lebron2016x4/build/bin/webserver
- The static files live at: /usr/src/projects/lebron2016x4/www/static
- To reach them from build/bin, you need to go up two directories hence: '../../path/to/files;'

> ✅ This is the correct config pathing for local development using a local config.

**Cloud Config Example:**
``` Nginx
location /static StaticHandler {
    root ./var/www/static;
}
```
- Note: During the Docker build, we copy files like this: \

**Docker Copy Step:**
``` Docker
COPY --from=builder /usr/src/project/www /var/www
```
- The binary and config are both in /usr/src/project, so './var/www/static is a valid relative path from the servery binary.

> ✅ This is the correct config pathing for production or Docker-based development using a cloud config.

> 📌 **Rule of Thumb:** The root path in your config is always interpreted relative to the location of the webserver binary at runtime, not the location of the config file. Make sure to adjust the path accordinly in local vs cloud enviornments.


# Building, Unit/Integration Testing, and Running the Server

> This project uses CMake with GoogleTest and Boost, and supports both standard CLI and Docker workflows.

## To Build Locally (No Docker):

> Make sure you are in a separate root/build/ directory to avoid in-source builds

In the CLI:

**From the /build directory:**
``` bash
    mkdir -p build && cd build  # Create build directory, enter into the build directory
    cmake ..                    # Generate build system
    make                        # Compile the binaries
    ctest --output-on-failure   # Run all unit + integration tests
```

- This should output in the CLI, all the results of the tests and integration test. If a compilation error occurs, you will see which file and where this error occured.

## To Build and Run with Docker
> Use this if you're deploying in a containerized enviornment or want consistency with production

From the project root /usr/src/projects/lebron2016x4 run:

``` bash
# Step 1: Build the base image
docker build -f docker/base.Dockerfile -t lebron2016x4:base .

# Step 2: Build the app image
docker build -f docker/Dockerfile -t my_image .

# Step 3: Run your container on port 8080 (local mapping)
docker run --rm -p 8080:8080 --name my_run my_image:latest          # IMPORTANT NOTE: 8080:8080 = local, 80:80 = cloud
```
If you'd like to explicitly specify a config:
``` bash
docker run --rm -p 8080:8080 my_image cloud_config
```

## Running Code Coverage Reports

### Local

> Make sure you are in a separarte root/build_coverage directory to avoid in-source builds

**From the /build_coverage directory:**
``` bash
    mkdir -p build_coverage                 # Create the build coverage directory
    cd build_coverage                       # Enter the build_coverage directory
    cmake -DCMAKE_BUILD_TYPE=Coverage ..    # Compile test coverage binaries
    make coverage                           # Generate the coverage report
```

- Output report will be available at:

> /usr/src/projects/lebron2016x4/build_coverage/report

- Within the build_coverage/report, you will see individual HTML files that show the coverage per file. The global coverage report is within 'index.html'

### Docker

**From the project root:**
``` bash
# Build and run with coverage instrumentation
docker build -f docker/coverage.Dockerfile -t coverage_image .

# Run the container to execute tests and generate coverage
docker run coverage_image
```

## Testing Overview
- All unit tests are written using GoogleTest in tests/
- An additional integration test (Python-based) is defined in tests/integration_test.py
- Any new additional tests must be added to the CMakeLists.txt in order for ctest to discover them
- You do not need to manually call test binaries - ctest discovers and runs them all

### Adding Tests
Begin by creating a new test source file based on the class name: 

For example, if you implement a new handler called MyHandler, you would create a test called:
> root/tests/myhandler_test.cc

Fully implement your test and add it to the CMakeList.txt:
> Do not forget to add '#include <gtest/gtest.h> within the header of the source file.
```
# ─────────────────────────────────────────────────────────────
#  (Tests & coverage placeholders)
# ─────────────────────────────────────────────────────────────
add_executable(unit_tests
  tests/server_test.cc
  tests/session_test.cc
  tests/config_parser_test.cc
  tests/request_test.cc
  tests/echo_handler_test.cc
  tests/static_handler_test.cc
  tests/not_found_handler_test.cc
  tests/router_test.cc
  tests/logger_test.cc
  tests/response_test.cc
  tests/handler_registry_test.cc
  test/myhandler_test.cc            # ADD NEW TEST HERE (FOR EXAMPLE)
)
```
Now you can repeat the build or build_coverage process and see the new result of your new test.

# Webserver Interaction

## Local Webserver Interaction
> Sometimes running all the unit and integration tests can biasly confirm functionality. You want to diligently test by running the actual server binary, interacting with it and testing the expected output via the console or via the local URL.

### Mapping Ports for Local Testing
**If using Google Cloud Shell:**

Before even running,
``` bash
cd cs130
tools/env/start.sh -u ${USER}
```
You need to map/expose additional ports arguments as they would be passed by 'docker run.' For example, to start the development enviornment and map the local port 8080 to port 8080 inside the development enviornment, run:
``` bash
cd cs130
tools/env/start.sh -u ${USER} -- -p 127.0.0.1:8080:8080
```

The '--' simply means we are passing extra arguments to 'docker run' where we want to specifically map our local IP address 127.0.0.1 to the development environments 8080:8080 port.

### Shutting Down

The development enviornment should shut down automatically, but if for some reason this doesn't happen run:
``` bash
docker container stop ${USER}_devel_env
```

If running the above command doesn't work, there might be some other issue. You can manually stop the development environment first by: finding your docker container ID and directly stopping it via the container ID.

To verify you're still in the development enviornment you should see on the CLI:

> {user}@{user}_devel_env:



To manually shut down the Docker process, we need to find the exact container ID that is running your development enviornment.

To find your Docker container's process ID, by running the following:

**Finding the Docker Container ID:**
``` bash
docker ps
```
**Example 'docker ps' output:**
``` bash
CONTAINER ID   IMAGE               COMMAND       CREATED         STATUS         PORTS                                                               NAMES
12345678901   <user>_devel_env   "/bin/bash"   3 minutes ago   Up 3 minutes   127.0.0.1:8080->8080/tcp, 7100-7110/tcp, 127.0.0.1:8085->8085/tcp   <user>_devel_env
```

**Manually stop the Docker Process:**
``` bash
docker stop 12345678901
```

This should successfully return you to your native bash shell.

## Testing Actual Behavior
Begin by going through the build or build_coverage process by creating the directories and compiling the binaries. However, be aware that where CMake is invoked is where the webserver binary will exist. So only run within the directory in which you invoked CMake from.

### Getting the Webserver Started Locally

**Map Local Port to Development Enviornment Port**
``` bash
$ tools/env/start.sh [args] -- -p 127.0.0.1:8080:8080   # Map your local port
```

**Create and Compile webserver binary via build || build_coverage**
``` bash
    mkdir -p build && cd build  # Create build directory, enter into the build directory
    cmake ..                    # Generate build system
    make                        # Compile the binaries
    ctest --output-on-failure   # Run all unit + integration tests
```

**Create a local config file for testing or use existing one:**
``` Nginx
# local_config
port 8080;

#Route paths must be ordered(most to least specific) due to longest-prefix matching

location /echo EchoHandler {
}

location /static StaticHandler {
    root ../../www/static;
}

location /public StaticHandler {
    root ../../www/static;
}

location / NotFoundHandler {
}
```

> The config must exist at the root directory, for you to declare the path as ../< config >

**From within the build || build_coverage directory, run the webserver binary via:**
``` bash
bin/webserver ../local_config
```

Now you should have the webserver binary running and actively listening for HTTP requests in that specific Terminal window. 

You should see the result of the config parser:
``` bash
TOKEN_TYPE_COMMENT: # Echo-server configuration
TOKEN_TYPE_NORMAL: port
TOKEN_TYPE_NORMAL: 8080
TOKEN_TYPE_STATEMENT_END: ;
TOKEN_TYPE_COMMENT: #Route paths must be ordered(most to least specific) due to longest-prefix matching
TOKEN_TYPE_NORMAL: location
TOKEN_TYPE_NORMAL: /echo
TOKEN_TYPE_NORMAL: EchoHandler
TOKEN_TYPE_START_BLOCK: {
TOKEN_TYPE_END_BLOCK: }
TOKEN_TYPE_NORMAL: location
TOKEN_TYPE_NORMAL: /static
TOKEN_TYPE_NORMAL: StaticHandler
TOKEN_TYPE_START_BLOCK: {
TOKEN_TYPE_NORMAL: root
TOKEN_TYPE_NORMAL: ../../www/static
TOKEN_TYPE_STATEMENT_END: ;
TOKEN_TYPE_END_BLOCK: }
TOKEN_TYPE_NORMAL: location
TOKEN_TYPE_NORMAL: /public
TOKEN_TYPE_NORMAL: StaticHandler
TOKEN_TYPE_START_BLOCK: {
TOKEN_TYPE_NORMAL: root
TOKEN_TYPE_NORMAL: ../../www/static
TOKEN_TYPE_STATEMENT_END: ;
TOKEN_TYPE_END_BLOCK: }
TOKEN_TYPE_NORMAL: location
TOKEN_TYPE_NORMAL: /
TOKEN_TYPE_NORMAL: NotFoundHandler
TOKEN_TYPE_START_BLOCK: {
TOKEN_TYPE_END_BLOCK: }
TOKEN_TYPE_EOF: 
```

As well as the actual server starting up:
``` bash
[2025-May-12 21:39:15.732932] [T:0x00007a4599a0b740] [info] Config file parsed: ../my_config (success)
[2025-May-12 21:39:15.733833] [T:0x00007a4599a0b740] [info] Instantiating handler 'EchoHandler' for location '/echo'
[2025-May-12 21:39:15.733968] [T:0x00007a4599a0b740] [info] Instantiating handler 'StaticHandler' for location '/static'
[2025-May-12 21:39:15.734058] [T:0x00007a4599a0b740] [info] Instantiating handler 'StaticHandler' for location '/public'
[2025-May-12 21:39:15.734173] [T:0x00007a4599a0b740] [info] Instantiating handler 'NotFoundHandler' for location '/'
[2025-May-12 21:39:15.734264] [T:0x00007a4599a0b740] [info] Server starting up on port 8080
[2025-May-12 21:39:15.734495] [T:0x00007a4599a0b740] [info] Server listening on port 8080
Server running on port 8080
```

### Interacting with the Webserver Locally

Now to test, open a second Terminal window.

**Viewing Echo Results in Terminal:**

```bash
nc localhost 8080
```

**If nc command is not found, simply install the NetCat packages:**
``` bash
apt-get install -y netcat
```

**Previewing/Interacting with the Webserver on Port 8080:**

Now you want to view what is actually being served, what HTTP results we are getting, or etc. Now, we lean on Cloud Shell Editor's live preview on whatever port we mapped to.

**To see how the webserver serves static files, you want to:**
- Navigate to the top right corner of Google Cloud Shell Editor and press the 'Web Preview' button. 
- Press 'Preview on port 8080'

You will be redirected to a webpage on the empty path URL. You will most likely see a '404: Not found' error. This is to be expected. 

**You will see your address bar looks similar to this:**

> https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/?authuser=1

**Which returns a:**

> 404 Not Found: The requested resource could not be found on this server.

**Establishing URL Paths:**

In the URL, you will see that before '?authuser=1' is the start of our file serving path. 

**To test behavior, remove that line and start checking out the output. For example:**

> https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/index.html

> https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.jpg

> https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.zip

> https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/test.txt

**You will also see the terminal running the server will update you with information on said HTTP requests:**
``` bash
[2025-May-12 21:43:33.086473] [T:0x00007a4599a0b740] [info] Accepted connection from 127.0.0.1
[2025-May-12 21:43:33.086815] [T:0x00007a4599a0b740] [info] New connection from 127.0.0.1
[2025-May-12 21:43:43.737469] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:43:43.737895] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:43:43.738349] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /?authuser=1 -> 404
[2025-May-12 21:43:44.046633] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:43:44.046895] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:43:44.048266] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /favicon.ico -> 404
[2025-May-12 21:43:58.346555] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:43:58.346782] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:43:58.365812] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /static/index.html -> 200
[2025-May-12 21:47:31.775513] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:47:31.775769] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:47:31.776607] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET / -> 404
[2025-May-12 21:47:34.391625] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:47:34.391887] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:47:34.392094] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /?authuser=1 -> 404
[2025-May-12 21:49:00.533147] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:49:00.533618] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:49:00.533893] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /?static/logo.jpg -> 404
[2025-May-12 21:49:05.801954] [T:0x00007a4599a0b740] [info] Accepted connection from 172.17.0.1
[2025-May-12 21:49:05.802195] [T:0x00007a4599a0b740] [info] New connection from 172.17.0.1
[2025-May-12 21:49:05.866286] [T:0x00007a4599a0b740] [info] 172.17.0.1 GET /static/logo.jpg -> 200
```

### Verification via Curl

**Confirming HTTP Reponse:**

You can confirm the HTTP response content in the second Terminal window you opened independent of the Terminal window running the webserver.

Here you can run curl commands to see the desired output:

**Simple Curl:**
``` bash
curl -I <URL-You-Are-Testing>
```

**Example:**
``` bash
curl -I https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.jpg
```

**Result:**
```bash
HTTP/1.1 302 Found
Content-Type: text/html; charset=utf-8
Date: Mon, 12 May 2025 21:58:55 GMT
Location: https://accounts.google.com/o/oauth2/v2/auth?client_id=618104708054-m0mqlm35l2ahieavnib6emtan2k95ps9.apps.googleusercontent.com&redirect_uri=https%3A%2F%2Fssh.cloud.google.com%2Fdevshell%2Fgateway%2Foauth&response_type=code&scope=email&state=eyJ0b2tlbiI6Im5DSUFMLTV5aC1lc0Y1RmxPSmJQb3ciLCJ0YXJnZXRfaG9zdCI6IjgwODAtY3MtN2ViM2VjMTQtYTYyMy00ZTI5LWJjN2ItYWE0MjY5ZjhmOWNkLmNzLXVzLXdlc3QxLWlqbHQuY2xvdWRzaGVsbC5kZXYiLCJhdXRodXNlciI6IiJ9
```

**Verbosed Curl:**
``` bash
curl -v <URL-You-Are-Testing>
```

**Example:**
``` bash
curl -v https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.jpg
```

**Result:**
```bash
* Host 8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev:443 was resolved.
* IPv6: (none)
* IPv4: 35.230.122.202
*   Trying 35.230.122.202:443...
* Connected to 8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev (35.230.122.202) port 443
* ALPN: curl offers h2,http/1.1
* TLSv1.3 (OUT), TLS handshake, Client hello (1):
*  CAfile: /etc/ssl/certs/ca-certificates.crt
*  CApath: /etc/ssl/certs
* TLSv1.3 (IN), TLS handshake, Server hello (2):
* TLSv1.3 (IN), TLS handshake, Encrypted Extensions (8):
* TLSv1.3 (IN), TLS handshake, Certificate (11):
* TLSv1.3 (IN), TLS handshake, CERT verify (15):
* TLSv1.3 (IN), TLS handshake, Finished (20):
* TLSv1.3 (OUT), TLS change cipher, Change cipher spec (1):
* TLSv1.3 (OUT), TLS handshake, Finished (20):
* SSL connection using TLSv1.3 / TLS_AES_128_GCM_SHA256 / X25519 / RSASSA-PSS
* ALPN: server accepted http/1.1
* Server certificate:
*  subject: CN=*.cs-us-west1-ijlt.cloudshell.dev
*  start date: Apr 11 07:30:04 2025 GMT
*  expire date: Jul 10 07:30:03 2025 GMT
*  subjectAltName: host "8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev" matched cert's "*.cs-us-west1-ijlt.cloudshell.dev"
*  issuer: C=US; O=Google Trust Services; CN=WR1
*  SSL certificate verify ok.
*   Certificate level 0: Public key type RSA (2048/112 Bits/secBits), signed using sha256WithRSAEncryption
*   Certificate level 1: Public key type RSA (2048/112 Bits/secBits), signed using sha256WithRSAEncryption
*   Certificate level 2: Public key type RSA (4096/152 Bits/secBits), signed using sha384WithRSAEncryption
* using HTTP/1.x
> GET /static/static.html HTTP/1.1
> Host: 8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev
> User-Agent: curl/8.5.0
> Accept: */*
> 
* TLSv1.3 (IN), TLS handshake, Newsession Ticket (4):
< HTTP/1.1 302 Found
< Content-Length: 469
< Content-Type: text/html; charset=utf-8
< Date: Mon, 12 May 2025 21:59:09 GMT
< Location: https://accounts.google.com/o/oauth2/v2/auth?client_id=618104708054-m0mqlm35l2ahieavnib6emtan2k95ps9.apps.googleusercontent.com&redirect_uri=https%3A%2F%2Fssh.cloud.google.com%2Fdevshell%2Fgateway%2Foauth&response_type=code&scope=email&state=eyJ0b2tlbiI6IjlRejlTRWhPXzNabmMwbGRFdnh1RXciLCJ0YXJnZXRfaG9zdCI6IjgwODAtY3MtN2ViM2VjMTQtYTYyMy00ZTI5LWJjN2ItYWE0MjY5ZjhmOWNkLmNzLXVzLXdlc3QxLWlqbHQuY2xvdWRzaGVsbC5kZXYiLCJhdXRodXNlciI6IiJ9
< 
<a href="https://accounts.google.com/o/oauth2/v2/auth?client_id=618104708054-m0mqlm35l2ahieavnib6emtan2k95ps9.apps.googleusercontent.com&amp;redirect_uri=https%3A%2F%2Fssh.cloud.google.com%2Fdevshell%2Fgateway%2Foauth&amp;response_type=code&amp;scope=email&amp;state=eyJ0b2tlbiI6IjlRejlTRWhPXzNabmMwbGRFdnh1RXciLCJ0YXJnZXRfaG9zdCI6IjgwODAtY3MtN2ViM2VjMTQtYTYyMy00ZTI5LWJjN2ItYWE0MjY5ZjhmOWNkLmNzLXVzLXdlc3QxLWlqbHQuY2xvdWRzaGVsbC5kZXYiLCJhdXRodXNlciI6IiJ9">Found</a>.

* Connection #0 to host 8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev left intact
```

### Testing Behavior on the Cloud Build
> The same way we were testing locally by modifying the URL path to see our desired output, we can do the same with the webserver hosted on Google Cloud Build.

**Cloud IP Address:**
> 34.169.145.188

**To test, just change the URL from before to our Cloud Build's IP Address:**
```
Previously:
curl -v https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.jpg
curl -v https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/index.html

Now:
curl -v https:34.169.145.188/static/logo.jpg
curl -v https:34.169.145.188/static/index.html
```

# Request Handler

## Request and Response API
> Before moving onto how our RequestHandlers are implements, first let's understand how the Request and Response API works. All handlers in this server operate on the same internal API: they receieve a Request object and must return a Respone object.

### Request Object
Defined in include/request.h, constructed from the raw HTTP input
``` cpp
class Request {
 public:
  std::string get_method() const;
  std::string get_url() const;
  std::string get_version() const;
  std::string get_header(const std::string& header_name) const;
  std::string to_string() const;
  bool is_valid() const;
  int length() const;
};
```
- get_url() - returns the full URL path of the incoming HTTP request (e.g., /static/index.html)
- get_method() - return the HTTP method string (GET, HEAD, etc.)
- get_version() - returns the HTTP version (e.g., "HTTP/1.1")
- get_header(name) - returns the value of the specified header, or empty string if missing
- is_valid() - returns false if the request line was malformed or missing.
- to_string() - returns the full raw request as a string
- length() - returns the number of character in the full request (used in testing to confirm content length)

### Response Object
The handler must return a Response constructed like this:
```cpp
Response(version, status_code, content_type, content_length, connection, body)
```

All parameters are strings except 'status_code' and 'content_length':
``` cpp
Response(
  "HTTP/1.1",        // version
  200,               // status code
  "text/html",       // content type
  1024,              // content length (in bytes)
  "close",           // connection
  "<html>...</html>" // body content
);
```

> 📌 All handlers must return a valid Response. There’s no global fallback if one is malformed.

## Existing Request Handler: StaticHandler

The StaticHandler serves files from the local filesystem in response to HTTP requests. It's mapped to routes in the config like /static or /public, and is one of the most commonly used handlers in the server.

### Location in Project:
```
Header: include/static_handler.h
Source: src/static_handler.cc
```

### What It Does:
When a request is made to a path that begins with /static, the server will:
- Match the route using longest-prefix matching.
- Instantiates a StaticHandler using its Init(...) method.
- Resolves the rest of the URL into a safe local file path.
- Reads the file if it exists and responds with the correct MIME type to serve proper files.
- Otherwise, it returns a 404/403 error.

### Registration:
StaticHandler is registered at load-time using a one-time inline declaration in the header:

**static_handler_.h**
``` cpp
inline bool _static_handler_registered =
    HandlerRegistry::RegisterHandler(
        StaticHandler::kName,
        StaticHandler::Init);
```

### Config Example:
``` Nginx
location /static StaticHandler {
    root ../../www/static;
}
```
- The path /static will trigger this handler.
- The root argument is mandatory and passed to the handler's constructor.
- Path resolution is relative to the server binary, not the config file.


### Factory Method:
``` cpp
RequestHandler* StaticHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params)
```
- Expects a "root" key in the parameters.
- Uses std::filesystem to resolve and canonicalize path relative to /proc/self/exe (the server binary's actual location)
- Returns a new instance of StaticHandler as a pointer.

### Constructor:
``` cpp
StaticHandler::StaticHandler(std::string url_prefix, std::string filesystem_root)
  : prefix_(std::move(url_prefix)),
    fs_root_(std::move(filesystem_root)) {}
```
- Stores both the URL prefix (e.g., /static) and the resolved path from the root directory for file serving.

### Path Resolution (Traversal Protection):
``` cpp
std::string StaticHandler::resolve_path(const std::string& url_path) const
```
- Removes the URL prefix to get the relative file path.
- Canonicalizes both the base root and the full resolved path.
- Ensured the resolved path still resides within the base to block attempts like '../../../etc/password'
- Throws on failure, which leads to a 403 Forbidden response.

### MIME Type Handling:
```cpp
std::string StaticHandler::get_extension(const std::string& path) const;
std::string StaticHandler::get_mime_type(const std::string& ext) const;
```
- Extracts file extensions like .html or .jpg or .zip
- Maps common types to proper Content-Type headers.
- Defaults to application/octet-stream for unknown types.

### Main Request Logic:
``` cpp
Response StaticHandler::handle_request(const Request& request)
```
- Calls resolve_path() to map the URL to a safe file path.
- Opens the file and streams it into a response body.
- Returns a '200 OK' with the file and appropriate MIIME Type to set the proper Content-Type.
- If the file is missing, returns '404 Not Found.'
- If traversal or mount violation is detected, returns a '403 Forbidden.'

### Importance of kName:
Every request handler **must** define a unique static name identifier. Such as:
``` cpp
static constexpr char kName[] = "StaticHandler";
```

This string acts as the lookup key used during config parsing. For example, when the config includes:
``` Nginx
location /static StaticHandler {
    root ../../www/static;
}
```

The server will match the string "StaticHandler" to the handler's kName parameter.

``` cpp
HandlerRegistry::RegisterHandler(StaticHandler::kName, StaticHandler::Init);
```

If the name does **not match exactly** between the config and the handler's kName, the handler will **not be instantiated**, and a '500 Internal Server Error' will be thrown when a request tries to access that route. 

> 📌 **Rule:** The string in the config file **must** match the kName defined in the class -- case sensitive and exact.

**Example: static_handler.cc:**
``` cpp
// src/static_handler.cc
#include "static_handler.h"
#include <fstream>
#include <iterator>
#include <cstring>

// define the kName symbol
constexpr char StaticHandler::kName[];                  // ***** THIS IS EXTREMELY IMPORTANT *****

// Factory invoked by HandlerRegistry
RequestHandler* StaticHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
  auto it = params.find("root");
  if (it == params.end()) {
    throw std::runtime_error(
      "StaticHandler missing 'root' parameter for location " + location);
  }

  // canonicalize the root on disk
  fs::path cfg = it->second;
  fs::path abs_root = cfg.is_absolute()
    ? fs::canonical(cfg)
    : fs::weakly_canonical(fs::read_symlink("/proc/self/exe").parent_path() / cfg);

  return new StaticHandler(location, abs_root.string());
}
...
```

**Example: static_handler.h:**
``` cpp
...

private:
  // Each handler instance needs exactly these two pieces of information:
  StaticHandler(std::string url_prefix, std::string filesystem_root);

  // The mount point (prefix) we were configured with.
  std::string prefix_;
  // The absolute filesystem root we were configured with.
  std::string fs_root_;

  // helpers
  std::string get_extension(const std::string& path) const;
  std::string get_mime_type(const std::string& ext) const;
  std::string resolve_path(const std::string& url_path) const;
};

// one-time registration at load time:
inline bool _static_handler_registered =            // ***** kName MUST be defined exactly as is in the config in order for proper registry! *****
    HandlerRegistry::RegisterHandler(
        StaticHandler::kName,
        StaticHandler::Init);

#endif  // STATIC_HANDLER_H
```


## Adding a New Request Handler

> To extend the server's functionality, you can define a custom request handler. Handlers are dynamically instantiated per request using a factory patten and can be registered declaratively. You should follow the design and pattern of the existing handlers such as StaticHandler or EchoHandler.

### Create Header and Source Files
Let's say you want to build a handler called MyHandler. Create the following files:
``` bash
include/my_handler.h
src/my_handler.cc
```

### Define the Handler Class in the Header
``` cpp
#ifndef MY_HANDLER_H
#define MY_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include <string>
#include <unordered_map>

class MyHandler : public RequestHandler {
 public:
  // Used as the lookup key in the config
  static constexpr char kName[] = "MyHandler";

  // Factory method used by the registry to create instances
  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);

  // Request handling logic
  Response handle_request(const Request& request) override;

 private:
  std::string prefix_;
  std::string my_arg_;

  // Constructor
  MyHandler(std::string location, std::string my_arg);
};

// One-time registration at load time
inline bool _my_handler_registered =
    HandlerRegistry::RegisterHandler(MyHandler::kName, MyHandler::Init);

#endif  // MY_HANDLER_H
```
- Do NOT forget to add the one-time registration line for load time registration.

> 🔑 kName must match exactly what you’ll use in the config. The config entry MyHandler will be resolved via this name.

### Implement the Handler Logic
``` cpp
#include "my_handler.h"

constexpr char MyHandler::kName[];

RequestHandler* MyHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
    
  // Pull a required parameter from the config (optional)
  auto it = params.find("my_arg");
  std::string arg = (it != params.end()) ? it->second : "default";

  return new MyHandler(location, arg);
}

MyHandler::MyHandler(std::string location, std::string my_arg)
  : prefix_(std::move(location)), my_arg_(std::move(my_arg)) {}

Response MyHandler::handle_request(const Request& request) {
  std::string body = "Hello from MyHandler. Arg = " + my_arg_;
  return Response(request.get_version(), 200, "text/plain", body.size(), "close", body);
}
```
Add whatever extra functionality you need for your specific handler.

### Add the Handler to the Appropriate Config
``` Nginx
location /my_handler MyHandler {
    root ./var/whatever/you/want/to/do;
}
```
- /my_handler is the URL route (must start with /)
- MyHandler is matched against MyHandler::kName
- The root argument will be passed as key-value pairs into the params map inside your hander's Init(...) method (or lack thereof, e.g. 404)

### Add the new Handler to the CMakeLists.txt
``` 
# ─────────────────────────────────────────────────────────────
#  Echo‑server static library
# ─────────────────────────────────────────────────────────────
add_library(echoserver_lib
  src/session.cc
  src/server.cc
  src/config_parser.cc
  src/request.cc
  src/response.cc
  src/echo_handler.cc
  src/static_handler.cc
  src/not_found_handler.cc
  src/router.cc
  src/logger.cc
  src/handler_registry.cc
  src/my_handler.cc                 # DO NOT FORGET TO ADD THIS OR YOUR NEW HANDLER WILL NOT BE COMPILED
)
```

### Add the header file to _main.cc
``` 
#include "echo_handler.h"
#include "static_handler.h"
#include "my_handler.h"

int main()...
```

### Rebuild and Run
``` bash
# Always clean stale builds
rm -r build && mkdir build && cd build
cmake ..
make
bin/webserver ../my_config
```

You should see logs similar to:
```
Instantiating handler 'MyHandler' for location '/my_handler'
Server starting up on port 8080
```

### Testing and Interaction
> After successfully compiling a new working handler, you should add unit tests and integration tests around them. Thoroughly test its functionality and it's code coverage. You must add new tests to the /test directory as well as updating the CMakeLists.txt accordingly. Then to test interaction, you can follow the steps from before except now:

**Interact with new URL:**
```
Old: https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/static/logo.jpg
New: https://8080-cs-7eb3ec14-a623-4e29-bc7b-aa4269f8f9cd.cs-us-west1-ijlt.cloudshell.dev/my_handler
```

Interact with the webserver to make sure it is behaving and outputting as desired. 

Your new handler should pass existing and new unit/integration tests as well.

### Final: Local to Cloud

Now move your local changes to the Cloud Build and check for functionality. If you are seeing unexpected behavior that was working working previously when locally testing, you might need to simply reset the VM after the Build Check. This quick reset will allow for the new handler to work. If this doesn't fix the unexpected behavior on the Cloud, then there was an issue elsewhere.

> 🎉 Congratulations! You just implemented a new working handler, tested it via unit testing, tested it via integation testing, and exposed a local port to confirm expected behavior.