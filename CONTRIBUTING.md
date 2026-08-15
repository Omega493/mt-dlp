# Contributing to mt-dlp

First off, thank you for considering contributing to `mt-dlp`! This project aims to be a robust, high-performance download utility, and we value technical precision and systems-level thinking.

Whether you're looking to refactor optimize thread logic, or fix a bug, your help is welcome!

## Development Workflow

**1.** **Fork the Project**: Create your own copy of the repository.
**2.** **Create your Feature Branch**: `git checkout -b feature/AmazingFeature`.
**3.** **Commit your Changes**: `git commit -m 'Add some AmazingFeature'`.
   - We appreciate signed commits (commits signed with a PGP signature). Please refer to [this video](https://www.youtube.com/watch?v=xj9OiJL56pM) on how to set a PGP signature on your device. (Disclaimer: Neither am I the creator of this video, nor am I related to them - this video serves only as a educational guide provided here.)
**4.** **Push to the Branch**: `git push origin feature/AmazingFeature`.
**5.** **Open a Pull Request**: Describe your changes clearly.
**6.** If you need file links for debugging various features, please check out `tests/tests.md`. It curates a bunch of "speedtest" links.

### Restricted Files (Vendored Code)
* **`include/selena/`**: Files in this directory belong to the standalone [Selena Library](https://codeberg.org/Omega493/selena).
  * **Do not edit these files directly in this repository.**
  * If you find a bug or want to add a feature to `selena`, please submit a Pull Request to the original [Selena repository](https://github.com/Omega493/selena).
  * We periodically sync this folder with the upstream version. Any local changes made here will be overwritten/rejected.

## Areas needing Help

If you are looking for a place to start, check the Issues tab or consider these architectural goals:

1. **Handling of Arguments**: Currently, the downloader works strictly on requesting a download URL from the user. Extend the logic to asking a filename, etc.
2. **Multi-file Support**: Extending the logic to handle a queue of multiple downloads.

## License

By contributing, you agree that your contributions will be licensed under the **GPLv3**.
When splitting code into new files (e.g., `src/ui.cpp`), you must copy the license header from main.cpp to the top of the new file.
