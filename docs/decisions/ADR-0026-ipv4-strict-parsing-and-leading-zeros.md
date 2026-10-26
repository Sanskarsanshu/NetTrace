# ADR-0026: Strict IPv4 Parsing, Rejection of Leading Zeros, and IPv6 Handling

- **Status:** Accepted
- **Date:** 2026-10-26
- **Context:** Milestone 2 (M2-A) and NETWORKING_RULES.md (D-15, D-01) require a strict IPv4 parser and canonical formatter.

## Context and Problem

Historical implementations of IP address parsing (e.g. BSD `inet_aton` and POSIX `inet_addr`) permitted octal numbers if an octet had a leading zero (such as `010.0.0.1` being interpreted as octal `8.0.0.1`), hexadecimal formats, and fewer than four octets (classful notation). Modern security analyses (e.g., IETF RFC 6943, WHATWG URL parsing standards, and common SSRF attack patterns) highlight that inconsistent parsing of leading zeros creates serious security vulnerabilities and cross-system interpretation discrepancies.

Furthermore, user inputs may mistakenly provide IPv6 addresses (e.g., `::1`, `fe80::1`) or invalid prefix notations when a bare IPv4 address is expected.

## Decision

1. **Strict Four-Octet Decimal Format:**
   - An IPv4 address must consist of exactly four decimal octets separated by single dots (`.`).
   - No leading, trailing, or embedded whitespace is tolerated.
   - Empty octets (`1..3.4`, `.1.2.3`, `1.2.3.`) are strictly rejected with `INVALID_ADDRESS`.
   - Octet values must be decimal integers in the range `[0, 255]`.

2. **Rejection of Leading Zeros:**
   - Any octet with two or more characters that begins with `'0'` (e.g. `01.2.3.4`, `192.168.01.1`) is strictly rejected with `INVALID_ADDRESS`.
   - A single `'0'` octet (e.g. `0.0.0.0`, `10.0.0.1`) is valid decimal zero.

3. **IPv6 and Unsupported Protocol Handling:**
   - Text containing IPv6 characteristics (specifically containing `:` such as `::1`, `2001:db8::1`) is rejected with `UNSUPPORTED_PROTOCOL` (or `INVALID_ADDRESS`) with a clear error message stating that IPv6 is not supported and IPv4 is required. It is never parsed or coerced into an IPv4 address.

4. **Bare Address vs Prefix Separation:**
   - A bare address parser strictly rejects strings containing `/` (e.g. `1.2.3.4/24`) with `INVALID_ADDRESS`. CIDR subnet parsing is handled by a distinct `IPv4Subnet` / prefix parser.

## Consequences

- Eliminates octal/decimal confusion and security ambiguities.
- Guarantees that parsing and formatting round-trips are lossless and bijective.
- Tested and locked by test `T-2-01`.
