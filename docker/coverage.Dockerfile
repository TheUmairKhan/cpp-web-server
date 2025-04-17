FROM gcr.io/lebron2016x4/lebron2016x4:base

WORKDIR /usr/src/projects/lebron2016x4

# Copy source code
COPY . .

# Create build coverage directory
RUN mkdir -p build_coverage

# Generate coverage report using our CMake macro
RUN cd build_coverage && \
    cmake -DCMAKE_BUILD_TYPE=Coverage .. && \
    make coverage

# Outputs the coverage summary into build logs when the container runs 
CMD cat build_coverage/report/index.html