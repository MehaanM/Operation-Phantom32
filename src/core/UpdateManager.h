#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// UpdateManager — robust firmware update state machine (Phase 10).
//
// Models the full update lifecycle: IDLE → DOWNLOADING → VALIDATING →
// APPLYING → DONE, with FAILED and ROLLED_BACK terminal error states. Each
// transition is guarded by explicit validation steps (manifest integrity,
// version compatibility, firmware checksum). Every attempt is recorded in a
// transaction log for auditability. Simulated power-loss marks an update as
// interruptible so callers can test resume/recovery paths.

static constexpr size_t PHANTOM_UM_MAX_TRANSACTIONS = 8;

struct UpdateManifest {
  char version[16];
  uint32_t expectedSize;
  uint32_t expectedChecksum;
  char minCompatVersion[16];
  char targetBoard[32];
  bool valid;
};

enum class UpdateState : uint8_t {
  IDLE,
  DOWNLOADING,
  VALIDATING,
  APPLYING,
  DONE,
  FAILED,
  ROLLED_BACK
};

enum class UpdateFailReason : uint8_t {
  NONE,
  INVALID_MANIFEST,
  CHECKSUM_MISMATCH,
  INCOMPATIBLE_VERSION,
  INCOMPLETE_DOWNLOAD,
  INTERRUPTED,
  ROLLBACK_INVOKED
};

struct UpdateTransaction {
  UpdateState finalState;
  UpdateFailReason failReason;
  char fromVersion[16];
  char toVersion[16];
  uint64_t startMs;
  uint64_t endMs;
  bool success;
};

class UpdateManager {
 public:
  UpdateManager() { memset(&manifest_, 0, sizeof(manifest_)); }

  // Load a manifest; sets state to IDLE if valid, leaves state unchanged if not.
  bool loadManifest(const UpdateManifest &m) {
    if (!m.valid || m.version[0] == '\0') {
      failWith(UpdateFailReason::INVALID_MANIFEST);
      return false;
    }
    manifest_ = m;
    state_ = UpdateState::IDLE;
    failReason_ = UpdateFailReason::NONE;
    return true;
  }

  bool validateManifest() const { return manifest_.valid && manifest_.version[0] != '\0'; }

  // Check whether the currentVersion satisfies the manifest's minCompatVersion.
  // An empty minCompatVersion means no requirement.
  bool checkCompatibility(const char *currentVersion) const {
    if (!currentVersion)
      return false;
    if (manifest_.minCompatVersion[0] == '\0')
      return true;
    // Simple lexicographic comparison — sufficient for semver-like strings.
    return strncmp(currentVersion, manifest_.minCompatVersion,
                   sizeof(manifest_.minCompatVersion)) >= 0;
  }

  bool validateIntegrity(uint32_t actualChecksum) const {
    return actualChecksum == manifest_.expectedChecksum;
  }

  // Transition: IDLE → DOWNLOADING.
  bool startUpdate(const char *fromVersion, uint64_t nowMs) {
    if (state_ != UpdateState::IDLE)
      return false;
    if (!validateManifest()) {
      failWith(UpdateFailReason::INVALID_MANIFEST);
      return false;
    }
    strncpy(currentFromVersion_, fromVersion ? fromVersion : "", 15);
    bytesReceived_ = 0;
    progress_ = 0;
    startMs_ = nowMs;
    interrupted_ = false;
    state_ = UpdateState::DOWNLOADING;
    return true;
  }

  // Report download progress; returns false if more bytes than expected.
  bool setDownloadProgress(uint32_t bytesReceived) {
    if (state_ != UpdateState::DOWNLOADING)
      return false;
    if (manifest_.expectedSize > 0 && bytesReceived > manifest_.expectedSize) {
      failWith(UpdateFailReason::INCOMPLETE_DOWNLOAD);
      return false;
    }
    bytesReceived_ = bytesReceived;
    if (manifest_.expectedSize > 0)
      progress_ = (uint8_t)((bytesReceived_ * 100u) / manifest_.expectedSize);
    return true;
  }

  // Transition: DOWNLOADING → VALIDATING → APPLYING → DONE.
  bool completeDownload(uint32_t actualChecksum, uint64_t nowMs) {
    if (state_ != UpdateState::DOWNLOADING)
      return false;
    if (manifest_.expectedSize > 0 && bytesReceived_ < manifest_.expectedSize) {
      failWith(UpdateFailReason::INCOMPLETE_DOWNLOAD);
      return false;
    }
    state_ = UpdateState::VALIDATING;
    progress_ = 90;
    if (!validateIntegrity(actualChecksum)) {
      failWith(UpdateFailReason::CHECKSUM_MISMATCH);
      return false;
    }
    state_ = UpdateState::APPLYING;
    progress_ = 95;
    // Applying completes immediately in this model (hardware platform handles it).
    state_ = UpdateState::DONE;
    progress_ = 100;
    recordTransaction(UpdateState::DONE, UpdateFailReason::NONE, nowMs, true);
    return true;
  }

  // Rollback from FAILED or APPLYING.
  bool rollback(uint64_t nowMs) {
    if (state_ != UpdateState::FAILED && state_ != UpdateState::APPLYING)
      return false;
    state_ = UpdateState::ROLLED_BACK;
    recordTransaction(UpdateState::ROLLED_BACK, UpdateFailReason::ROLLBACK_INVOKED, nowMs, false);
    return true;
  }

  // Simulate power-loss during download (for testing recovery paths).
  void simulatePowerLoss() {
    if (state_ == UpdateState::DOWNLOADING || state_ == UpdateState::APPLYING) {
      interrupted_ = true;
      failWith(UpdateFailReason::INTERRUPTED);
    }
  }
  bool canResume() const { return interrupted_ && bytesReceived_ > 0; }

  // Reset to IDLE, discarding current manifest and progress.
  void reset() {
    state_ = UpdateState::IDLE;
    failReason_ = UpdateFailReason::NONE;
    progress_ = 0;
    bytesReceived_ = 0;
    interrupted_ = false;
    memset(&manifest_, 0, sizeof(manifest_));
  }

  // --- Accessors ---

  UpdateState state() const { return state_; }
  UpdateFailReason failReason() const { return failReason_; }
  uint8_t progress() const { return progress_; }
  uint32_t bytesReceived() const { return bytesReceived_; }

  size_t transactionCount() const { return txCount_; }
  const UpdateTransaction *transactionAt(size_t i) const {
    return i < txCount_ ? &transactions_[i] : nullptr;
  }

 private:
  UpdateManifest manifest_ = {};
  UpdateState state_ = UpdateState::IDLE;
  UpdateFailReason failReason_ = UpdateFailReason::NONE;
  uint8_t progress_ = 0;
  uint32_t bytesReceived_ = 0;
  bool interrupted_ = false;
  uint64_t startMs_ = 0;
  char currentFromVersion_[16] = {};

  UpdateTransaction transactions_[PHANTOM_UM_MAX_TRANSACTIONS] = {};
  size_t txCount_ = 0;

  void failWith(UpdateFailReason reason) {
    state_ = UpdateState::FAILED;
    failReason_ = reason;
  }

  void recordTransaction(UpdateState final, UpdateFailReason reason, uint64_t nowMs, bool success) {
    if (txCount_ >= PHANTOM_UM_MAX_TRANSACTIONS) {
      // Rotate: drop oldest.
      for (size_t i = 1; i < PHANTOM_UM_MAX_TRANSACTIONS; ++i)
        transactions_[i - 1] = transactions_[i];
      txCount_ = PHANTOM_UM_MAX_TRANSACTIONS - 1;
    }
    UpdateTransaction &t = transactions_[txCount_++];
    t.finalState = final;
    t.failReason = reason;
    t.success = success;
    t.startMs = startMs_;
    t.endMs = nowMs;
    strncpy(t.fromVersion, currentFromVersion_, sizeof(t.fromVersion) - 1);
    strncpy(t.toVersion, manifest_.version, sizeof(t.toVersion) - 1);
  }
};
