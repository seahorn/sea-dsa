# Docker build for SeaDsa

Use the following command
```
docker build --build-arg UBUNTU=xenial --build-arg BUILD_TYPE=Release -t seahorn_xenial_rel -f docker/seahorn-full-size-rel.Dockerfile .
```

Depends on docker build for SeaHorn. More details in SeaHorn repository.
