# Antivirus False Positives FAQ

**English** · [简体中文](ANTIVIRUS_FAQ.zh-CN.md)

## In short

- FLiNG Downloader is open-source software, licensed under the GNU AGPL v3.
- Windows Defender and other antivirus products may flag it through heuristic detection.
- A detection does not mean the app contains malicious code. Check where your copy came from and that it is intact (see below).

## Why it gets flagged

- The app downloads files, extracts them and writes many files to disk, which is behavior heuristics watch for.
- The executable is statically linked, so its size and characteristics are more likely to trigger detection models.
- A new release has been seen by few users, so it has not built up a reputation score yet.

## What to do

1. Download the app only from the official Releases page:
   https://github.com/Sqhh99/FLiNG-Downloader/releases
2. Verify the file: compare the SHA-256 of the zip or setup file with the `SHA256SUMS.txt` attached to the same release.
3. Add the app's folder to the Defender exclusions, then run it.
4. If it is still blocked, submit the file to Microsoft as a false positive.

## Adding a Windows Defender exclusion

1. Open **Windows Security** → **Virus & threat protection**.
2. Under **Virus & threat protection settings**, select **Manage settings**.
3. Under **Exclusions**, select **Add or remove exclusions**.
4. Add the folder that contains the app.

## Reporting a false positive to Microsoft

Submit the file here:
https://www.microsoft.com/en-us/wdsi/filesubmission

Include, if you can:

- the detection name (for example `Win32/Wacapew.C!ml`);
- your Windows version and Defender engine version;
- where the file came from (the official Release link);
- the steps that trigger the detection, with screenshots.

## Other antivirus products

- In general, add the app's folder to the product's exclusions, allow list or exceptions.
- If you can't find the setting, search the product's help for "exclude folder" or "allow list".

## Security and privacy

- The app collects no personally identifiable information.
- It uses the network only to search for trainers, download files, and check for app and database updates.
- Settings, caches and logs are stored only on your computer.

## Where to get help

- Security issues (private):
  https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new
- Questions about using the app:
  https://github.com/Sqhh99/FLiNG-Downloader/discussions
- Bugs:
  https://github.com/Sqhh99/FLiNG-Downloader/issues

Last updated: 2026-10-19
