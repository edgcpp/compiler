struct Payload { unsigned char data[9]; int tag; };
Payload step_payload(Payload in, int delta) {
  for (int i = 0; i < 9; ++i) {
    in.data[i] = static_cast<unsigned char>(in.data[i] + delta + i);
  }
  in.tag += delta * 2;
  return in;
}
int risky_div(int a, int b) {
  if (b == 0) throw 137;
  return a / b;
}
