FROM ubuntu:24.04

RUN cat /etc/lsb-release

# Default to Azure mirror (what GitHub Actions ubuntu runners use)
RUN sed -i -e 's/http:\/\/archive\.ubuntu\.com\/ubuntu\//http:\/\/azure.archive.ubuntu.com\/ubuntu\//' /etc/apt/sources.list.d/ubuntu.sources

RUN mkdir -p /scripts
COPY get-dependencies_linux.sh /scripts
RUN chmod +x /scripts/get-dependencies_linux.sh

RUN apt-get -u update \
 && DEBIAN_FRONTEND=noninteractive apt-get -y install gcc g++ clang libc-dev dpkg-dev ninja-build pkg-config file \
 && /scripts/get-dependencies_linux.sh ubuntu build-dependencies \
 && rm -rf /var/lib/apt/lists/*

# Install Vulkan SDK
ENV VULKANSDK_SHA256 79b0a1593dadc46180526250836f3e53688a9a5fb42a0e5859eb72316dc4d53e
ADD "https://sdk.lunarg.com/sdk/download/1.3.296.0/linux/vulkansdk-linux-x86_64-1.3.296.0.tar.xz?Human=true" "/tmp"
RUN echo "$VULKANSDK_SHA256 /tmp/vulkansdk-linux-x86_64-1.3.296.0.tar.xz" | sha256sum -c \
 && mkdir /vulkan \
 && tar -C /vulkan -xf /tmp/vulkansdk-linux-x86_64-1.3.296.0.tar.xz \
 && rm /tmp/vulkansdk-linux-x86_64-1.3.296.0.tar.xz
ENV VULKAN_SDK="/vulkan/1.3.296.0/x86_64"
ENV PATH="$PATH:$VULKAN_SDK/bin"

# Defines arguments which can be passed during build time.
ARG USER=warzone2100
ARG UID=1000
# Create a user with given UID.
RUN useradd -d /home/warzone2100 -m -g root -u "$UID" "$USER"
# Switch to warzone2100 user by default.
USER warzone2100
# Check the current uid of the user.
RUN id

WORKDIR /code
CMD ["sh"]

