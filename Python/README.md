Splunk custom command to convert IPs to human-readable server names using reverse DNS and whois information.

- [ip_cached_lookup.py](ip_cached_lookup.py)

  Lookup implementation.

  IP address is checked for reverse DNS availability (gethostbyaddr). If there is no reverse DNS name obtained then whois services are used.
  
  To speed up consequient lookups results obtained are cached in the SQLite database.
  
  To prevent whois services abuse, cooldown counters stored in the same SQLite database are used.

- [rdns.py](rdns.py)

  Integration of the reverse DNS/whois lookup call into the Splunk data processing pipeline.
