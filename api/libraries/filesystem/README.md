# Filesystem

> Abstract filesystem interface — file I/O, directory iteration, and filesystem-level operations over any underlying storage backend.

<!--
  Suggested image: block diagram showing the application → iFileSystem interface →
  backend layer (LittleFS / FatFS / SPIFFS) → storage driver (NVM / SD / Flash).
  Recommended size: 900×320 px.

  ![Filesystem block diagram](../../../docs/img/filesystem_diagram.png)
-->

---

## Features

- File open/close with `FileMode` (ReadOnly, WriteOnly, ReadWrite, Append)
- Blocking read and write using `ByteArray` / `ConstByteArray`
- Seek, tell, size and flush per file handle
- Directory create, remove, open, iterate (readDir loop), close
- Filesystem-level stat, exists, remove, rename, mount, unmount, format
- Opaque `uint32_t` handles — no raw pointers exposed to the caller
- No dynamic allocation; the implementation manages a fixed internal handle pool

---

## Header

```cpp
#include <hel_iFileSystem>
```

---

## Types

### `FileMode`

| Value | Behaviour |
|---|---|
| `ReadOnly` | Open existing file for reading; error if not found |
| `WriteOnly` | Create file or truncate existing; write-only |
| `ReadWrite` | Open existing file for reading and writing |
| `Append` | Open or create; all writes go to end of file |

### `SeekOrigin`

| Value | Description |
|---|---|
| `Begin` | Offset relative to file start |
| `Current` | Offset relative to current position |
| `End` | Offset relative to file end |

### `FileStat`

| Field | Type | Description |
|---|---|---|
| `size` | `uint32_t` | File size in bytes (0 for directories) |
| `is_directory` | `bool` | `true` if the entry is a directory |

### `DirEntry`

| Field | Type | Description |
|---|---|---|
| `name` | `char[256]` | Null-terminated entry name (not full path) |
| `size` | `uint32_t` | Entry size in bytes (0 for directories) |
| `is_directory` | `bool` | `true` if the entry is a directory |

### Handles

| Constant | Value | Meaning |
|---|---|---|
| `kInvalidFileHandle` | `0U` | Uninitialized or closed file handle |
| `kInvalidDirHandle` | `0U` | Uninitialized or closed directory handle |

---

## API

### Mount / unmount

| Method | Description |
|---|---|
| `mount()` | Mount the filesystem; must be called before any file operation |
| `unmount()` | Flush all writes and release all handles |
| `format()` | Erase and reinitialize the filesystem (optional) |

### File operations

| Method | Description |
|---|---|
| `open(path, mode, handle)` | Open a file; returns an opaque `FileHandle` |
| `close(handle)` | Close file and flush pending writes |
| `read(handle, buffer, bytes_read)` | Read up to `buffer.size()` bytes |
| `write(handle, data)` | Write all bytes in `data` |
| `seek(handle, offset, origin)` | Move file position |
| `tell(handle, position)` | Read current file position |
| `size(handle, size)` | Read file size in bytes |
| `flush(handle)` | Flush write cache to storage |

`read` returns `ErrorEndOfFile` when the position is already at the end of the file (`bytes_read == 0`).

### Filesystem-level

| Method | Description |
|---|---|
| `remove(path)` | Delete a file |
| `rename(old_path, new_path)` | Rename or move a file or directory |
| `exists(path, result)` | Check if a path exists |
| `stat(path, stat)` | Get size and type for any path |

### Directory operations

| Method | Description |
|---|---|
| `mkdir(path)` | Create a directory (final component only) |
| `rmdir(path)` | Remove an empty directory |
| `openDir(path, handle)` | Open directory for iteration |
| `readDir(handle, entry)` | Read the next entry; `ErrorEndOfFile` when done |
| `closeDir(handle)` | Close directory handle |

---

## Usage examples

### Writing a log file

```cpp
#include <hel_iFileSystem>
#include <hel_string>

fs.mount();

hel::StringData<32> path;
path = "/logs/run.log";

hel::FileHandle fh = hel::kInvalidFileHandle;
fs.open(path, hel::FileMode::Append, fh);

static const uint8_t msg[] = "boot\n";
fs.write(fh, hel::ConstByteArray(msg, sizeof(msg) - 1U));

fs.close(fh);
```

### Reading a configuration file

```cpp
hel::StringData<32> path;
path = "/config.bin";

uint8_t cfg_buf[512];
hel::FileHandle fh = hel::kInvalidFileHandle;

if (fs.open(path, hel::FileMode::ReadOnly, fh) == hel::ReturnCode::AnsweredRequest)
{
    uint32_t bytes_read = 0U;
    fs.read(fh, hel::ByteArray(cfg_buf, sizeof(cfg_buf)), bytes_read);
    fs.close(fh);
    parseConfig(cfg_buf, bytes_read);
}
```

### Listing directory contents

```cpp
hel::StringData<8> root;
root = "/data";

hel::DirHandle dh = hel::kInvalidDirHandle;
fs.openDir(root, dh);

hel::DirEntry entry;
while (fs.readDir(dh, entry) == hel::ReturnCode::AnsweredRequest)
{
    if (!entry.is_directory)
    {
        hel::String name(entry.name);  // non-allocating view
        // name.size(), name.c_str(), name == "log.txt", ...
    }
}
fs.closeDir(dh);
```

### Checking disk usage before writing

```cpp
hel::StringData<32> path;
path = "/data/capture.bin";

hel::FileStat st;
if (fs.stat(path, st) == hel::ReturnCode::AnsweredRequest)
{
    // st.size = current file size
}

path = "/calibration.bin";
bool found = false;
fs.exists(path, found);
if (!found)
{
    // generate defaults
}
```

---

## Implementing for a new target

Map each virtual method to the underlying filesystem library (LittleFS, FatFS, etc.):

```cpp
class LittleFs : public hel::iFileSystem
{
public:
    hel::ReturnCode mount() noexcept override
    {
        const int rc = lfs_mount(&m_lfs, &m_cfg);
        return (rc == 0) ? hel::ReturnCode::AnsweredRequest
                         : hel::ReturnCode::ErrorGeneral;
    }

    hel::ReturnCode open(const hel::String& path,
                          hel::FileMode      mode,
                          hel::FileHandle&   handle) noexcept override
    {
        // map FileMode → LFS_O_RDONLY / LFS_O_WRONLY / LFS_O_CREAT etc.
        // use path.c_str() to pass to the underlying C library
        // allocate slot from m_file_pool[], assign handle = slot index + 1
    }

    hel::ReturnCode read(hel::FileHandle   handle,
                          hel::ByteArray    buffer,
                          uint32_t&         bytes_read) noexcept override
    {
        lfs_file_t* f = handleToFile(handle);
        const lfs_ssize_t n = lfs_file_read(&m_lfs, f,
                                             buffer.data(),
                                             static_cast<lfs_size_t>(buffer.size()));
        if (n < 0)    { return hel::ReturnCode::ErrorReadFailed; }
        if (n == 0)   { return hel::ReturnCode::ErrorEndOfFile;  }
        bytes_read = static_cast<uint32_t>(n);
        return hel::ReturnCode::AnsweredRequest;
    }

    // ... remaining methods

private:
    lfs_t           m_lfs;
    lfs_config      m_cfg;
    lfs_file_t      m_file_pool[8];  // fixed-size handle pool
};
```

---

## Thread-safety

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances (e.g. two separate filesystems on different storage devices).
- All methods must **not** be called from ISR context.
