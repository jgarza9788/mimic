# Forking from Fluxbox checklist

Mimic is planned as a Fluxbox-based fork. This document tracks migration work.

## Environment note

- GitHub access to `https://github.com/fluxbox/fluxbox` was blocked in this execution environment (HTTP 403 tunnel failure).
- Because of this, this repository currently contains Mimic-first scaffolding that is intentionally shaped to receive Fluxbox source integration in the next milestone.

## Renamed artifacts completed in this repo

- Project name: `Mimic`
- Main executable target: `mimic`
- Session entry: `Mimic` (`sessions/mimic.desktop`)
- Config sample: `config/mimic.keys`

## Intentional pending items once Fluxbox sources are imported

- Preserve upstream copyrights and legal notices in all imported files
- Keep explicit attribution to Fluxbox/Blackbox lineage in README/AUTHORS/COPYING
- Rename user-facing docs/man pages/install metadata from Fluxbox to Mimic
- Audit remaining `Fluxbox` strings and classify as:
  - legal attribution (keep)
  - historical comments (likely keep)
  - user-facing branding (rename)

## Tracking TODOs

- [ ] Import Fluxbox upstream source into Mimic repo
- [ ] Port first-wave modules onto Fluxbox event plumbing
- [ ] Add compatibility layer for Fluxbox key format and future TOML config
- [ ] Expand test matrix against imported tree
