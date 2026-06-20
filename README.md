# ETP - Emergency Transfer Protocol

## Overview
Emergency Transfer Protocol (ETP) is a **Custom Transport Layer Protocol** built from the ground up as a robust alternative to TCP, UDP, and QUIC. It is specifically designed to guarantee **lossless and effective data transmission** in extreme network conditions such as areas suffering from high latency, massive packet loss, or infrastructure damage caused by natural disasters and war.

---

## Writeup
A full writeup was compiled of the project outlining existing solutions and their problems, a detailed specification of the project, and testing results. This can be found in *ETP Writeup.pdf.*

---

## Features

- **Lossless Data Transmission** Guarantees data arrival even at extreme packet drop rates, solving the fundamental unreliability flaws of UDP.

- **High Throughput in Extreme Conditions** Maintains efficiency on unstable networks where traditional TCP suffers from severe throughput drops due to Head-of-Line Blocking and false congestion assumptions.

- **Independent Architecture** Built entirely from scratch. Because it is not layered over existing transport protocols (unlike QUIC, which relies on UDP), it avoids being inadvertently blocked by firewalls trying to prevent UDP-based malware attacks.

- **Lightweight & Efficient** Avoids the heavy CPU/GPU overhead of forced transport-layer encryption. Furthermore, the protocol utilizes an ultra-compact **12-byte header** (nearly half the minimum size of TCP) to maximize payload efficiency.

- **Optimized 2-Way Handshake** Reduces network traffic by replacing TCP's 3-way handshake with an optimized 2-way handshake (SYN1 → SYN1-ACK) that immediately establishes connections and begins data transmission.

- **Intelligent Packet Recovery** Uses "packet mini-sets" alongside specialized **RES packets** to precisely re-request only the missing packets, minimizing unnecessary retransmissions.

---

## Architecture Overview

- **C++** is used to implement the protocol logic and the custom network simulation environment.
- **Custom Packet Structure:** Operates within a standard IPv6 MTU (1500B), reserving 1460B for ETP. It uses a **12-byte header** and allows up to **1448B for the payload**.
- **Header Components:**
  - **Ports:** 2 bytes each for Source and Destination.
  - **Sequence Number:** 4 bytes, allowing up to $2^{31}$ packets per session and enabling easy recipient reordering.
  - **Checksum:** 2 bytes dedicated to hashing the packet to ensure data integrity.
  - **Flags:** 1 byte utilizing specific bit toggles (`END`, `STA`, `FIN`, `URG`, `RES`, `ACK`, `SYN`) to compactly control flow and connection states.
- **Session Management:** Connection persistence is managed via custom Window Sizes and application-defined Round Trip Times (RTT). Handshakes gracefully initialize (`SYN`), transfer (`STA`/`RES`), and terminate (`FIN`/`FIN-END`) sessions.
- **Simulation & Testing Environment:** The protocol's effectiveness was benchmarked using a custom C++ simulator that altered mean latency and packet drop rates to generate a performance heatmap in kbps.**Python and SQL** were used to then generate the heatmap shown in the writeup.

---

## Learning Results

- I learnt more about the **OSI Model**, specifically the heavy responsibilities and limitations of the Transport Layer.
- I identified and analysed the structural trade-offs in modern network protocols (TCP, UDP, QUIC).
- I improved my **C++ skills** by developing a comprehensive testing environment and network simulator to benchmark my protocol.
- I gained practical experience in **packet architecture and byte-level manipulation**, learning how to optimize header structures to maximize the Maximum Segment Size (MSS) and lower network congestion.

--- 

## Future Improvements

- **Real-World OS Integration** to test ETP on actual hardware network interfaces.
- **Adaptive Congestion Control** algorithms to dynamically adjust window sizes based on live RTT and packet drop feedback whilst still being more efficient than TCP.
- **Optional Application-Layer Encryption** to provide security for defence against adversarial attacks on the network. 
- **Expanded Language Support** by creating wrappers or rewrites in languages like Rust or Python to increase implementation availability