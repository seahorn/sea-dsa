# Docker build for SeaDsa #

Based on SeaHorn docker. For details, see `docker/README.md` in the corresponding [SeaHorn](https://github.com/seahorn/seahorn) repo.

## Building instructions ## 

Use the following command

```
docker build --build-arg BUILD_TYPE=RelWithDebInfo -t seahorn/sea-dsa-builder:jammy-llvm14 -f docker/sea-dsa-builder.Dockerfile .
```

