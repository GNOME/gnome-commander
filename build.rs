fn main() {
    for dep in ["gcmd-main", "gviewer", "gcmd-tags", "gcmd", "stdc++"] {
        println!("cargo:rustc-link-lib={dep}");
    }
    // allow plugins to see symbols
    println!("cargo:rustc-link-arg=-rdynamic");
    system_deps::Config::new().probe().unwrap();
}
