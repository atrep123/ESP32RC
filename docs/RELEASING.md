# ESP32RC Releasing Guide

## Scope

This document describes how to publish a GitHub Release for ESP32RC.

## Current release assets

- Tag example: `v1.1.0`
- Changelog: `CHANGELOG.md`
- Optional checked-in release notes: `release-notes/<tag>.md`
- Release workflow: `.github/workflows/release.yml`

## Standard release flow

1. Make sure `main` is clean and pushed.
2. Update `CHANGELOG.md`.
3. Create the release commit.
4. Create and push an annotated tag:

```bash
git tag -a vX.Y.Z -m "ESP32RC X.Y.Z"
git push origin main
git push origin vX.Y.Z
```

5. If `release-notes/vX.Y.Z.md` exists, the release workflow will use it.
6. Otherwise the workflow can generate GitHub release notes automatically.

## Publishing options

### Option 1: Automatic on tag push

Pushing a tag that matches `v*` triggers `.github/workflows/release.yml`.

Expected result:

- A GitHub Release is created for the tag.
- The checked-in notes file is used when present.
- Otherwise GitHub-generated notes are used.

### Option 2: Manual workflow dispatch

Use this when:

- The tag already exists
- The workflow was added after the tag was pushed
- You want to republish from the GitHub UI

In GitHub:

1. Open `Actions`
2. Select `Release`
3. Click `Run workflow`
4. Enter the tag, for example `v1.1.0`
5. Start the workflow

## Notes for v1.1.0

The repository already contains:

- Tag `v1.1.0`
- Checked-in notes at `release-notes/v1.1.0.md`

If the release does not yet exist on GitHub, run the `Release` workflow manually with tag `v1.1.0`.
