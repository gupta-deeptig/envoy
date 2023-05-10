FROM golang:alpine@sha256:0d145ecb3cb3772ee54d3a97ae2774aa4f8a179f28f9d4ea67b9cb38b58acebd AS builder

RUN apk --no-cache add make
COPY . /app
RUN make -C /app/grpc-service

FROM alpine@sha256:02bb6f428431fbc2809c5d1b41eab5a68350194fb508869a33cb1af4444c9b11

COPY --from=builder /app/grpc-service/server /app/server
CMD ["/app/server", "-users", "/etc/users.json"]
