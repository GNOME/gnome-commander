fn main() {
    println!("cargo:rustc-link-lib=gcmd");
    system_deps::Config::new().probe().unwrap();
}
