#include "srv_config.hxx"

namespace SDN_Raft {

ptr<srv_config> srv_config::deserialize(Buffer& buf) {
    BufferSerializer bs(buf);
    return deserialize(bs);
}

ptr<srv_config> srv_config::deserialize(BufferSerializer& bs) {
    int32 id = bs.get_i32();
    int32 dc_id = bs.get_i32();
    const char* endpoint_char = bs.get_cstr();
    const char* aux_char = bs.get_cstr();
    std::string endpoint( (endpoint_char) ? endpoint_char : std::string() );
    std::string aux( (aux_char) ? aux_char : std::string() );
    byte is_learner = bs.get_u8();
    int32 priority = bs.get_i32();
    return cs_new<srv_config>(id, dc_id, endpoint, aux, is_learner, priority);
}

ptr<Buffer> srv_config::serialize() const{
    ptr<Buffer> buf = Buffer::alloc(
        sz_int +
        sz_int +
        (endpoint_.length()+1) +
        (aux_.length()+1) +
        1 +
        sz_int
    );
    buf->put(id_);
    buf->put(dc_id_);
    buf->put(endpoint_);
    buf->put(aux_);
    buf->put((byte)(learner_?(1):(0)));
    buf->put(priority_);
    buf->pos(0);
    return buf;
}

} // namespace SDN_Raft;
