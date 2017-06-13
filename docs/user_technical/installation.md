## Installation

Memgraph is a 64-bit Linux compatible database management system.
For the purpose of Alpha testing Memgraph has been packed into
Ubuntu 16.04 based [Docker](https://www.docker.com) image.
Before proceeding with the installation, please install
the Docker engine on your system.
Instructions how to install Docker can be found
[here](https://docs.docker.com/engine/installation).
Memgraph Docker image was built with Docker version `1.12`,
so all Docker versions since version `1.12` should work.

### Import

After a successful download, Memgraph can be imported as follows
```
docker load -i /path/to/<memgraph_docker_image_name>.tar.gz
```

### Run

The most convenient way to start Memgraph is
```
docker run -d -p 7687:7687 --name <memgraph_docker_container_name> <memgraph_docker_image_name>
```
`-d` means that the container will be detached (run in the background mode).
Because the default Bolt protocol port is `7687`, the straightforward option
is to run Memgraph on that port.

### Configuration Parameters

Memgraph can be run with various parameters. The parameters should be
appended at the end of `docker run` command in the following format
`--param-name=param-value`.
Below is a list of all available parameters

 Name  | Type | Default | Description
-------|------|:-------:|-------------
 port | integer | 7687 | Communication port on which to listen.
 num_workers | integer | 8 |  Number of workers (concurrent threads).
 snapshot_cycle_sec | integer | 300 | Interval, `in seconds`, between two database snapshots. Value of -1 turns the snapshots off. 
 max_retained_snapshots | integer | 3 | Number of retained snapshots, -1 means without limit.
 snapshot_on_db_destruction | bool | false | Make a snapshot when closing Memgraph.
 recover_on_startup | bool | false | Recover the database on startup.

To find more about how to execute queries against
the database please proceed to [Quick Start](quick-start.md).

### Cleanup

Status & Memgraph's logging messages can be checked with:
```
docker ps -a
docker logs -f <memgraph_docker_container_name>
```

To stop Memgraph, execute
```
docker stop <memgraph_docker_container_name>
```

After the container has been stopped, it can be removed by
executing
```
docker rm <memgraph_docker_container_name>
```