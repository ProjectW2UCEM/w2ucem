#include "w2ucem/util/error.hpp"

namespace w2ucem {

const char* error_code_str(ErrorCode code) {
  switch (code) {
    case ErrorCode::None: return "none";
    case ErrorCode::Io: return "io";
    case ErrorCode::Parse: return "parse";
    case ErrorCode::SchemaViolation: return "schema-violation";
    case ErrorCode::EvaluationError: return "evaluation-error";
    case ErrorCode::HashMismatch: return "hash-mismatch";
    case ErrorCode::SignatureInvalid: return "signature-invalid";
    case ErrorCode::Network: return "network";
    case ErrorCode::MirrorExhausted: return "mirror-exhausted";
    case ErrorCode::ContainerNotFound: return "container-not-found";
    case ErrorCode::ContainerFailure: return "container-failure";
    case ErrorCode::VerificationFailed: return "verification-failed";
    case ErrorCode::Unsupported: return "unsupported";
  }
  return "unknown";
}

}  // namespace w2ucem
