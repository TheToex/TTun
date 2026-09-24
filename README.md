# TTun — Toex Tunnel

**TTun (Toex Tunnel)** is a lightweight and high-performance **Layer 3 UDP tunnel** written in pure C for Linux.

It provides an interactive CLI manager that makes it easy to create, configure, and manage multiple tunnels without dealing with complex commands manually.

## ✨ Features

* ⚡ **High Performance** — Core engine written in pure C with minimal overhead.
* 🌐 **Layer 3 Forwarding** — Intercepts and forwards raw IP packets through virtual **TUN interfaces**.
* 🖥️ **Interactive CLI Manager** — Manage your tunnels through a simple and user-friendly Bash interface using the `ttun` command.
* 🔀 **Multi-Tunnel Support** — Run multiple tunnels simultaneously (`TTun`, `TTun1`, `TTun2`, ...).
* 📦 **Automatic Installation** — Supports major Linux distributions out of the box:

  * Ubuntu
  * Debian
  * Arch Linux
  * CentOS / Fedora
* ⚙️ **Systemd Integration** — Tunnels can run as background `systemd` services.
* 🛠️ **Easy Management** — Create, start, stop, restart, and remove tunnels from the interactive manager.

---

## ⚡ Quick Install

Install TTun with a single command:

```bash
curl -fsSL https://raw.githubusercontent.com/TheToex/TTun/main/install.sh | sudo bash
```

Run this command on **both the server and client machines**.


## 🚀 Usage

Launch the interactive TTun manager:

```bash
sudo ttun
```

The interactive menu allows you to:

### Create a Tunnel

The wizard will guide you through the required configuration:

1. Select the tunnel role:

   * **Server**
   * **Client**
2. Configure the required IP addresses and ports.
3. TTun automatically generates the configuration.
4. A virtual TUN interface is created.
5. The interface is assigned its virtual IP address.
6. The tunnel is started as a `systemd` service.

The default TUN interface name is:

```text
TTun
```

### Manage Tunnels

From the same interactive menu, you can:

* View all configured tunnels
* Check tunnel status
* Start tunnels
* Stop tunnels
* Restart tunnels
* Delete tunnels
* Manage multiple tunnels independently

---

## 🌐 Example Network Topology

TTun creates a virtual Layer 3 network between the server and client.

By default, the virtual network uses:

| Device | TUN Address   |
| ------ | ------------- |
| Server | `10.0.0.1/24` |
| Client | `10.0.0.2/24` |

### Connection Test

Once the tunnel is running on both sides, you can test connectivity using the virtual IP addresses.

**On the Client:**

```bash
ping 10.0.0.1
```

**On the Server:**

```bash
ping 10.0.0.2
```

If the tunnel is working correctly, both sides should be able to reach each other's virtual TUN address.

---

## 🔎 Checking the TUN Interface

You can inspect the TTun interface and its assigned IP address with:

```bash
ip addr show TTun
```

For multiple tunnels, the interface names may look like:

```text
TTun
TTun1
TTun2
TTun3
...
```

You can also list all TUN interfaces with:

```bash
ip link show
```

---

## 🏗️ Architecture

TTun operates at **Layer 3** using Linux TUN interfaces.

```text
┌──────────────┐                         ┌──────────────┐
│    Client    │                         │    Server    │
│              │                         │              │
│  Application │                         │  Application │
│      │       │                         │      ▲       │
│      ▼       │                         │      │       │
│   TTun TUN   │                         │   TTun TUN   │
│  10.0.0.2/24 │                         │  10.0.0.1/24 │
│      │       │                         │      ▲       │
│      ▼       │      UDP Tunnel         │      │       │
│   TTun Core  │════════════════════════▶│   TTun Core  │
│      │       │◀════════════════════════│      │       │
└──────────────┘                         └──────────────┘
```

The TUN interface provides a virtual Layer 3 network device, while TTun transports IP packets between the endpoints over UDP.

---

## 📂 Project Structure

A typical TTun installation contains:

```text
ttun/
├── install.sh
├── src/
│   └── ...
├── config/
│   └── ...
└── README.md
```

The installer handles the required setup and integration with the system.

---

## 🐧 Supported Distributions

TTun is designed to work out of the box on:

* Ubuntu
* Debian
* Arch Linux
* Fedora
* CentOS

Other Linux distributions may work as well, provided they support:

* Linux TUN interfaces
* `systemd`
* Bash
* Standard networking utilities

---

## 📋 Requirements

* Linux
* Root / `sudo` access
* Bash
* `systemd`
* TUN/TAP support
* GCC or another C compiler during installation

---

## 📜 License

TTun is released under the **MIT License**.


---

<p align="center">
  Made with C & by <b>TheToex</b>
</p>
