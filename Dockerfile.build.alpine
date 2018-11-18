FROM suilteam/mecury:alpine
MAINTAINER "Carter Mbotho <carter@suilteam.com>"

# Copy Binaries
COPY artifacts/ /usr/
COPY suil/sut        /usr/local/bin/sut
COPY buildapp   /usr/local/bin/
RUN  chmod +x   /usr/local/bin/buildapp

# Add Suilteam certificate
COPY suilteam.ddns.net.crt /usr/local/share/ca-certificates/
RUN  update-ca-certificates

# Remove unnecessary installations
RUN apk del ca-certificates

# User to use for building
RUN adduser -s /bin/sh -h /home/build/ -D build

WORKDIR /home/build