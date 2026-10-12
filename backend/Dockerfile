FROM rust:1-bookworm AS build
WORKDIR /src
COPY Cargo.toml Cargo.lock ./
COPY src src
COPY migrations migrations
RUN cargo build --release --locked

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && rm -rf /var/lib/apt/lists/* \
    && useradd -m -u 1000 nw
COPY --from=build /src/target/release/nw-core /usr/local/bin/nw-core
USER nw
EXPOSE 8090
ENTRYPOINT ["nw-core"]
CMD ["serve"]
