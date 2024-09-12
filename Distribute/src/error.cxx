namespace SDN_Raft {

const char * raft_err_msg[] = {
    "OK",
    "N1: Error",
    "N2: Leader receive AppendEntriesRequest from another leader with same term.",
    "N3: Remove this server from cluster and step down.",
    "N4: Illegal state: leader shouldn't encounter election timeout.",
    "N5: Unexpected message for response.",
    "N6: No Snapshot could be found while no config can be found in committed logs.",
    "N7: There must be a configuration at log index one.",
    "N8: Peer's lastLogIndex is too large.",
    "N9: Receive an unknown request type.",
    "N10: Leader receive InstallSnapshotRequest from another leader with same term.",
    "N11: Not follower for applying a Snapshot.",
    "N12: Failed to apply the Snapshot after log compacted.",
    "N13: Failed to handle Snapshot installation due to system errors.",
    "N14: SnapshotSyncContext must not be null.",
    "N15: Received an unexpected response message type.",
    "N16: Failed to find a Snapshot for peer.",
    "N17: Empty Snapshot.",
    "N18: Partial Snapshot block read.",
    "N19: Bad log_idx for retrieving the term value.",
    "N20: Background committing thread encounter err.",
    "N21: Log store flush failed.",
    "N22: This node does not get messages from leader, while the others do.",
    "N23: Commit is invoked before pre-commit, order inversion happened."
};

} 

