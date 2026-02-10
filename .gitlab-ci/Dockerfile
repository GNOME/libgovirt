FROM fedora:43

# NOTE: We set tsflags to empty so docs actually install...
RUN dnf update -y \
    && dnf builddep -y libgovirt \
    && dnf clean all

ARG HOST_USER_ID=5555
ENV HOST_USER_ID=${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG=C.UTF-8
