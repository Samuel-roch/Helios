/**
 ******************************************************************************
 * @file    iFileSystem.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.1.0
 * @date    2026-05-07
 * @ingroup HELIOS_LIB_FILESYSTEM
 * @brief   Abstract filesystem interface.
 *
 * @details
 *   - File open/close with @ref FileMode (ReadOnly, WriteOnly, ReadWrite, Append)
 *   - Blocking read and write using @ref ByteArray / @ref ConstByteArray
 *   - Seek, tell, size and flush per file handle
 *   - Directory creation, iteration and removal
 *   - Filesystem-level stat, exists, remove, rename, format and mount/unmount
 *   - All paths are passed as @ref String references — no raw pointers in the
 *     public API
 *   - All handles are opaque @c uint32_t values; @ref kInvalidFileHandle and
 *     @ref kInvalidDirHandle mark uninitialized or closed handles
 *   - No dynamic allocation; the implementation manages a fixed internal
 *     handle pool sized at construction
 *
 * @note
 *   All paths use forward-slash separators and start from the root (e.g.
 *   @c "/data/log.txt").  The root directory is @c "/".
 *   Methods are not thread-safe on the same instance; the caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_LIB_iFileSystem_HPP_
#define HELIOS_LIB_iFileSystem_HPP_

#include <hel_string>
#include <hel_return_code>
#include <cstdint>
#include <cstddef>

namespace hel
{

// @formatter:off

// =============================================================================
// Constants
// =============================================================================

/** @brief Sentinel value for an uninitialized or closed file handle. */
static constexpr uint32_t kInvalidFileHandle = 0U;

/** @brief Sentinel value for an uninitialized or closed directory handle. */
static constexpr uint32_t kInvalidDirHandle  = 0U;

/** @brief Maximum length of a single path component including the null terminator. */
static constexpr std::size_t kFsMaxNameLength = 256U;

// =============================================================================
// FileHandle / DirHandle
// =============================================================================

/**
 * @brief Opaque file handle returned by @ref iFileSystem::open.
 * @ingroup HELIOS_LIB_FILESYSTEM
 * @details A value of @ref kInvalidFileHandle indicates the handle is not valid.
 */
using FileHandle = uint32_t;

/**
 * @brief Opaque directory handle returned by @ref iFileSystem::openDir.
 * @ingroup HELIOS_LIB_FILESYSTEM
 * @details A value of @ref kInvalidDirHandle indicates the handle is not valid.
 */
using DirHandle = uint32_t;

// =============================================================================
// FileMode
// =============================================================================

/**
 * @enum  FileMode
 * @brief Access mode for @ref iFileSystem::open.
 * @ingroup HELIOS_LIB_FILESYSTEM
 */
enum class FileMode : uint8_t
{
  ReadOnly  = 0x00U, /*!< Open existing file for reading; error if not found.           */
  WriteOnly = 0x01U, /*!< Create file or truncate existing file; open for writing only. */
  ReadWrite = 0x02U, /*!< Open existing file for reading and writing; error if not found. */
  Append    = 0x03U  /*!< Open or create file; all writes go to end of file.            */
};

// =============================================================================
// SeekOrigin
// =============================================================================

/**
 * @enum  SeekOrigin
 * @brief Reference point for @ref iFileSystem::seek.
 * @ingroup HELIOS_LIB_FILESYSTEM
 */
enum class SeekOrigin : uint8_t
{
  Begin   = 0x00U, /*!< Offset is relative to the start of the file.  */
  Current = 0x01U, /*!< Offset is relative to the current position.   */
  End     = 0x02U  /*!< Offset is relative to the end of the file.    */
};

// @formatter:on

// =============================================================================
// FileStat
// =============================================================================

/**
 * @struct FileStat
 * @brief  Metadata for a file or directory entry returned by @ref iFileSystem::stat.
 * @ingroup HELIOS_LIB_FILESYSTEM
 */
struct FileStat
{
  uint32_t size;         /*!< File size in bytes; 0 for directories.  */
  bool     is_directory; /*!< @c true if the entry is a directory.    */
};

// =============================================================================
// DirEntry
// =============================================================================

/**
 * @struct DirEntry
 * @brief  Single directory entry returned by @ref iFileSystem::readDir.
 * @ingroup HELIOS_LIB_FILESYSTEM
 *
 * @details The @c name field is a fixed-size null-terminated character array.
 *   A read-only @ref String view over it can be obtained without allocation:
 *   @code
 *   String view(entry.name);   // non-allocating view; valid while entry is alive
 *   @endcode
 */
struct DirEntry
{
  char     name[kFsMaxNameLength]; /*!< Null-terminated entry name (not full path). */
  uint32_t size;                   /*!< Entry size in bytes; 0 for directories.     */
  bool     is_directory;           /*!< @c true if the entry is a directory.        */
};

// =============================================================================
// iFileSystem
// =============================================================================

/**
 * @class  iFileSystem
 * @brief  Hardware-agnostic abstract filesystem interface.
 * @ingroup HELIOS_LIB_FILESYSTEM
 *
 * @details
 *   - Mount and unmount the underlying filesystem with @ref mount and @ref unmount.
 *   - Open files with @ref open; the resulting @ref FileHandle is used for all
 *     subsequent file operations.  Close with @ref close when done.
 *   - Read bytes into a @ref ByteArray with @ref read; the actual byte count is
 *     returned in @p bytes_read.  Returns @ref ReturnCode::ErrorEndOfFile when
 *     the read position is already at the end of the file.
 *   - Write from a @ref ConstByteArray with @ref write; all bytes must be
 *     written or an error is returned.
 *   - Seek within a file with @ref seek; query the current position with @ref tell.
 *   - Iterate a directory by calling @ref openDir, then @ref readDir in a loop
 *     until @ref ReturnCode::ErrorEndOfFile is returned, then @ref closeDir.
 *   - All path parameters are passed as @c const @ref String references.
 *   - No dynamic allocation; the implementation manages handle pools internally.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   the caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; filesystem instances are singletons owned by the BSP layer.
 */
class iFileSystem
{
public:
  virtual ~iFileSystem() noexcept = default;

  // -------------------------------------------------------------------------
  // Mount / unmount
  // -------------------------------------------------------------------------

  /**
   * @brief  Mount the filesystem.
   * @details Initializes the underlying storage driver and loads the filesystem
   *   metadata.  Must be called before any file or directory operation.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Mounted successfully.
   *   - @ref ReturnCode::ErrorReadFailed  : Storage could not be read.
   *   - @ref ReturnCode::ErrorGeneral     : Filesystem metadata is corrupt.
   */
  [[nodiscard]]
  virtual ReturnCode mount() noexcept = 0;

  /**
   * @brief  Unmount the filesystem.
   * @details Flushes all cached writes and releases all internal resources.
   *   All open handles are invalidated.  Calling @ref mount again is required
   *   before further use.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Unmounted successfully.
   *   - @ref ReturnCode::NotInitialized   : Filesystem was not mounted.
   *   - @ref ReturnCode::ErrorWriteFailed : Flush of pending writes failed.
   */
  [[nodiscard]]
  virtual ReturnCode unmount() noexcept = 0;

  /**
   * @brief  Erase and reinitialize the filesystem.
   * @details Destroys all data on the storage device and writes a new empty
   *   filesystem.  The filesystem does not need to be mounted first.
   *   Returns @ref ReturnCode::ErrorNotSupported if the implementation does not
   *   expose a format operation.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Format successful.
   *   - @ref ReturnCode::ErrorWriteFailed  : Storage write failed during format.
   *   - @ref ReturnCode::ErrorNotSupported : Implementation does not support format.
   */
  [[nodiscard]]
  virtual ReturnCode format() noexcept = 0;

  // -------------------------------------------------------------------------
  // File operations
  // -------------------------------------------------------------------------

  /**
   * @brief      Open a file and return a handle.
   * @param[in]  path    Absolute path (e.g. @c "/data/log.txt").
   * @param[in]  mode    Access mode — see @ref FileMode.
   * @param[out] handle  Set to the opened @ref FileHandle on success.
   *   Initialized to @ref kInvalidFileHandle on failure.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : File opened; @p handle is valid.
   *   - @ref ReturnCode::NotInitialized    : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound : File does not exist (@ref FileMode::ReadOnly or @ref FileMode::ReadWrite).
   *   - @ref ReturnCode::ErrorPathNotFound : Intermediate directory does not exist.
   *   - @ref ReturnCode::ErrorDiskFull     : No space to create the file.
   *   - @ref ReturnCode::FunctionBusy      : Handle pool is exhausted.
   */
  [[nodiscard]]
  virtual ReturnCode open(const String& path, FileMode mode, FileHandle& handle) noexcept = 0;

  /**
   * @brief     Close an open file handle and flush pending writes.
   * @param[in] handle  Handle returned by @ref open.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : File closed successfully.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorWriteFailed   : Final flush of write cache failed.
   */
  [[nodiscard]]
  virtual ReturnCode close(FileHandle handle) noexcept = 0;

  /**
   * @brief      Read bytes from an open file.
   * @details    Reads up to @c buffer.size() bytes starting at the current
   *   file position and advances the position by @p bytes_read.
   * @param[in]  handle      File handle.
   * @param[out] buffer      Destination buffer.
   * @param[out] bytes_read  Number of bytes actually read; may be less than
   *   @c buffer.size() only at end of file.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Bytes read; @p bytes_read is valid.
   *   - @ref ReturnCode::ErrorEndOfFile     : File position was already at the end; @p bytes_read is 0.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorReadFailed    : Storage read error.
   */
  [[nodiscard]]
  virtual ReturnCode read(FileHandle handle,
                          ByteArray   buffer,
                          uint32_t&   bytes_read) noexcept = 0;

  /**
   * @brief     Write bytes to an open file.
   * @details   Attempts to write all bytes in @p data starting at the current
   *   file position.  The position is advanced by @c data.size() on success.
   *   In @ref FileMode::Append mode the write is always performed at the end
   *   of the file regardless of the current seek position.
   * @param[in] handle  File handle.
   * @param[in] data    Source buffer; data is not modified.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : All bytes written successfully.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorInvalidState  : File was opened as @ref FileMode::ReadOnly.
   *   - @ref ReturnCode::ErrorDiskFull      : Insufficient space; no bytes written.
   *   - @ref ReturnCode::ErrorWriteFailed   : Storage write error.
   */
  [[nodiscard]]
  virtual ReturnCode write(FileHandle handle, ConstByteArray data) noexcept = 0;

  /**
   * @brief     Move the file read/write position.
   * @param[in] handle  File handle.
   * @param[in] offset  Signed byte offset relative to @p origin.
   * @param[in] origin  Reference point — see @ref SeekOrigin.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Position updated.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorParam         : Resulting position would be negative.
   */
  [[nodiscard]]
  virtual ReturnCode seek(FileHandle handle,
                          int32_t    offset,
                          SeekOrigin origin) noexcept = 0;

  /**
   * @brief      Read the current file position.
   * @param[in]  handle    File handle.
   * @param[out] position  Current byte offset from the start of the file.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : @p position is valid.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   */
  [[nodiscard]]
  virtual ReturnCode tell(FileHandle handle, uint32_t& position) noexcept = 0;

  /**
   * @brief      Read the size of an open file.
   * @param[in]  handle  File handle.
   * @param[out] size    File size in bytes on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : @p size is valid.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorReadFailed    : Storage read error.
   */
  [[nodiscard]]
  virtual ReturnCode size(FileHandle handle, uint32_t& size) noexcept = 0;

  /**
   * @brief     Flush write-cache contents to storage.
   * @details   Ensures that data written via @ref write is committed to the
   *   underlying storage device.  Close implicitly flushes.
   * @param[in] handle  File handle.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Data flushed.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorWriteFailed   : Storage write error during flush.
   */
  [[nodiscard]]
  virtual ReturnCode flush(FileHandle handle) noexcept = 0;

  // -------------------------------------------------------------------------
  // Filesystem-level operations
  // -------------------------------------------------------------------------

  /**
   * @brief     Delete a file.
   * @param[in] path  Absolute path to the file.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : File deleted.
   *   - @ref ReturnCode::NotInitialized    : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound : File does not exist.
   *   - @ref ReturnCode::ErrorNotAFile     : @p path refers to a directory; use @ref rmdir.
   *   - @ref ReturnCode::ErrorWriteFailed  : Storage write error.
   */
  [[nodiscard]]
  virtual ReturnCode remove(const String& path) noexcept = 0;

  /**
   * @brief     Rename or move a file or directory.
   * @param[in] old_path  Source path.
   * @param[in] new_path  Destination path.
   * @details   If @p new_path exists and is a file it is overwritten.  Moving
   *   across different filesystem roots is not supported.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Renamed successfully.
   *   - @ref ReturnCode::NotInitialized    : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound : @p old_path does not exist.
   *   - @ref ReturnCode::ErrorPathNotFound : Parent directory of @p new_path does not exist.
   *   - @ref ReturnCode::ErrorWriteFailed  : Storage write error.
   */
  [[nodiscard]]
  virtual ReturnCode rename(const String& old_path, const String& new_path) noexcept = 0;

  /**
   * @brief      Check whether a path exists.
   * @param[in]  path    Absolute path.
   * @param[out] result  Set to @c true if the path exists (file or directory).
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p result is valid.
   *   - @ref ReturnCode::NotInitialized  : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorReadFailed : Storage read error.
   */
  [[nodiscard]]
  virtual ReturnCode exists(const String& path, bool& result) noexcept = 0;

  /**
   * @brief      Retrieve metadata for a file or directory.
   * @param[in]  path  Absolute path.
   * @param[out] stat  Populated with size and type on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p stat is valid.
   *   - @ref ReturnCode::NotInitialized    : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound : Path does not exist.
   *   - @ref ReturnCode::ErrorReadFailed   : Storage read error.
   */
  [[nodiscard]]
  virtual ReturnCode stat(const String& path, FileStat& stat) noexcept = 0;

  // -------------------------------------------------------------------------
  // Directory operations
  // -------------------------------------------------------------------------

  /**
   * @brief     Create a directory.
   * @details   Creates only the final component; intermediate directories must
   *   already exist.
   * @param[in] path  Absolute path for the new directory.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest        : Directory created.
   *   - @ref ReturnCode::NotInitialized         : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileExists        : A file or directory already exists at @p path.
   *   - @ref ReturnCode::ErrorPathNotFound      : Parent directory does not exist.
   *   - @ref ReturnCode::ErrorDiskFull          : No space to create the directory.
   *   - @ref ReturnCode::ErrorWriteFailed       : Storage write error.
   */
  [[nodiscard]]
  virtual ReturnCode mkdir(const String& path) noexcept = 0;

  /**
   * @brief     Remove an empty directory.
   * @param[in] path  Absolute path.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest        : Directory removed.
   *   - @ref ReturnCode::NotInitialized         : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound      : Directory does not exist.
   *   - @ref ReturnCode::ErrorNotADirectory     : @p path refers to a file; use @ref remove.
   *   - @ref ReturnCode::ErrorDirectoryNotEmpty : Directory is not empty.
   *   - @ref ReturnCode::ErrorWriteFailed       : Storage write error.
   */
  [[nodiscard]]
  virtual ReturnCode rmdir(const String& path) noexcept = 0;

  /**
   * @brief      Open a directory for iteration.
   * @param[in]  path    Absolute path to the directory.
   * @param[out] handle  Set to the opened @ref DirHandle on success.
   *   Initialized to @ref kInvalidDirHandle on failure.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Directory opened; @p handle is valid.
   *   - @ref ReturnCode::NotInitialized     : Filesystem is not mounted.
   *   - @ref ReturnCode::ErrorFileNotFound  : Directory does not exist.
   *   - @ref ReturnCode::ErrorNotADirectory : @p path refers to a file.
   *   - @ref ReturnCode::FunctionBusy       : Handle pool is exhausted.
   */
  [[nodiscard]]
  virtual ReturnCode openDir(const String& path, DirHandle& handle) noexcept = 0;

  /**
   * @brief      Read the next entry from an open directory.
   * @details    Entries are returned in implementation-defined order.  The
   *   @c "." and @c ".." entries are omitted.  Returns
   *   @ref ReturnCode::ErrorEndOfFile when all entries have been read.
   *   A @ref String view over @c entry.name can be obtained with:
   *   @code
   *   String name(entry.name);
   *   @endcode
   * @param[in]  handle  Directory handle returned by @ref openDir.
   * @param[out] entry   Populated with the next entry on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : @p entry is valid; more entries may follow.
   *   - @ref ReturnCode::ErrorEndOfFile     : No more entries; iteration is complete.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   *   - @ref ReturnCode::ErrorReadFailed    : Storage read error.
   */
  [[nodiscard]]
  virtual ReturnCode readDir(DirHandle handle, DirEntry& entry) noexcept = 0;

  /**
   * @brief     Close an open directory handle.
   * @param[in] handle  Directory handle returned by @ref openDir.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Directory closed.
   *   - @ref ReturnCode::ErrorInvalidHandle : @p handle is not open.
   */
  [[nodiscard]]
  virtual ReturnCode closeDir(DirHandle handle) noexcept = 0;

protected:
  /** @brief Deleted — filesystem instances are non-copyable singletons. */
  iFileSystem& operator=(const iFileSystem&) = delete;
  /** @brief Deleted — filesystem instances are non-movable singletons. */
  iFileSystem& operator=(iFileSystem&&) = delete;
};

} // namespace hel

#endif // HELIOS_LIB_iFileSystem_HPP_
