#!/bin/sh
set -eu

IMAGE=ark-os-test

if [ -n "$(docker images -q "$IMAGE")" ]; then
    docker rmi -f "$IMAGE" >/dev/null 2>&1 || true
fi

docker buildx build --platform linux/amd64 --load -t "$IMAGE" .

echo "Running startup test..."
docker run --rm "$IMAGE" true

echo "Running command override test..."
docker run --rm "$IMAGE" /bin/sh -c 'echo hello' > /tmp/noah-cmd.out
grep -qx 'hello' /tmp/noah-cmd.out

echo "Running init override test..."
docker run --rm -e NOAH_INIT='echo init-ok' "$IMAGE" > /tmp/noah-init.out
grep -qx 'init-ok' /tmp/noah-init.out

echo "Running non-root runtime test..."
docker run --rm --user 1000:1000 "$IMAGE" /bin/sh -c 'id -u && id -g' > /tmp/noah-nonroot.out
[ "$(sed -n '1p' /tmp/noah-nonroot.out)" = "1000" ]
[ "$(sed -n '2p' /tmp/noah-nonroot.out)" = "1000" ]

echo "Running shutdown test..."
cat <<'EOF' > /tmp/noah-test.sh
#!/bin/sh
sleep 1
EOF
chmod +x /tmp/noah-test.sh

docker run --rm -d --name noah_test "$IMAGE" sh -c 'sleep 30'
# Wait for the container to start
sleep 2

docker stop noah_test >/dev/null

docker rm -f noah_test >/dev/null

echo "All tests passed."
